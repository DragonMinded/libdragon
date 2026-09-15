/**
 * Regression for ULC mono output saturation.
 *
 * Synthetic Q1.14 inputs exercise the RSP overlay: mono output must equal
 * clamp16(2 * unscaled output), with identical retained lap state.
 *
 * After rebuilding and installing libdragon: make -C tests testrom_ulc.z64
 * Run on hardware or an accurate emulator; expect ULC_TEST_PASS in IS Viewer.
 */
#include <libdragon.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

DEFINE_RSP_UCODE(rsp_ulc);

#define TRANSFORM 0x80000000u
#define LAP       0x40000000u
#define DISCARD   0x10000000u
#define SCALE     0x08000000u

static uint32_t overlay;
static int failures, cases;
static int16_t *coeff, *state, *out, *reference, *saved_state;

static const int16_t edges[] = {
    INT16_MIN, -20000, -16385, -16384, -16383, -1, 0, 1,
    16382, 16383, 16384, 16385, 20000, INT16_MAX, -8192, 8192,
};

static int16_t clamp16(int value)
{
    return value < INT16_MIN ? INT16_MIN : value > INT16_MAX ? INT16_MAX : value;
}

static void init_inputs(int seed, bool quiet)
{
    uint32_t rng = seed + 1;
    for (int i = 0; i < 1024; i++) {
        rng = rng * 1664525u + 1013904223u;
        coeff[i] = ((int)(rng >> 16) - 32768) / (quiet ? 512 : 16);
    }
    for (int i = 0; i < 512; i++)
        state[i] = edges[(i + seed) % 16] / (quiet ? 8 : 1);
    for (int i = 0; i < 2048; i++) out[i] = 0x1234;
}

static void synthesize(int size, int overlap, uint32_t flags)
{
    rspq_write(overlay, 0, PhysicalAddr(coeff), PhysicalAddr(state),
        PhysicalAddr(out), flags | LAP | ((uint32_t)overlap << 16) | size);
    rspq_wait();
}

static void check_case(int size, int overlap, bool transform, bool quiet, int seed)
{
    int bad = 0, positive = 0, negative = 0;
    uint32_t flags = transform ? TRANSFORM : 0;
    init_inputs(seed, quiet);
    synthesize(size, overlap, flags);
    memcpy(reference, out, size * sizeof(*out));
    memcpy(saved_state, state, 512 * sizeof(*state));
    init_inputs(seed, quiet);
    synthesize(size, overlap, flags | SCALE);
    for (int i = 0; i < size; i++) {
        int expected = clamp16(2 * (int)reference[i]);
        positive += reference[i] >= 16384;
        negative += reference[i] < -16384;
        if (out[i] != expected) {
            if (!bad) debugf("ULC_MISMATCH size=%d overlap=%d transform=%d quiet=%d i=%d got=%d want=%d\n",
                size, overlap, transform, quiet, i, out[i], expected);
            bad++;
        }
    }
    // Mono conversion must not clip the persistent Q1.14 overlap state.
    bad += memcmp(saved_state, state, 512 * sizeof(*state)) != 0;
    for (int i = size; i < 2048; i++) bad += out[i] != 0x1234;
    // Every loud case must actually exercise both polarities of overrange.
    if (!quiet && (!positive || !negative)) bad++;
    if (quiet && (positive || negative)) bad++;
    failures += bad != 0;
    cases++;
    debugf("ULC_CASE size=%d overlap=%d transform=%d quiet=%d seed=%d positive=%d negative=%d bad=%d\n",
        size, overlap, transform, quiet, seed, positive, negative, bad);

    // Discarded/preroll output still updates state, but must not touch output.
    init_inputs(seed, quiet);
    synthesize(size, overlap, flags | SCALE | DISCARD);
    bad = memcmp(saved_state, state, 512 * sizeof(*state)) != 0;
    for (int i = 0; i < 2048; i++) bad += out[i] != 0x1234;
    failures += bad != 0;
    cases++;
}

static void check_stereo(void)
{
    // Mid/side interleaving already saturates; preserve it at the boundaries.
    int16_t *mid = coeff, *side = reference;
    for (int i = 0; i < 1024; i++) {
        mid[i] = edges[i % 16];
        side[i] = edges[(i / 16) % 16];
    }
    rspq_write(overlay, 1, PhysicalAddr(mid), PhysicalAddr(side), PhysicalAddr(out));
    rspq_wait();
    int bad = 0;
    for (int i = 0; i < 1024; i++) {
        bad += out[i*2] != clamp16(2 * (int)clamp16(mid[i] + side[i]));
        bad += out[i*2+1] != clamp16(2 * (int)clamp16(mid[i] - side[i]));
    }
    debugf("ULC_STEREO bad=%d\n", bad);
    failures += bad != 0;
    cases++;
}

int main(void)
{
    debug_init_isviewer();
    console_init();
    console_set_render_mode(RENDER_MANUAL);
    rspq_init();
    overlay = rspq_overlay_register(&rsp_ulc);
    coeff = malloc_uncached(2048);
    state = malloc_uncached(1024);
    out = malloc_uncached(4096);
    reference = malloc_uncached(2048);
    saved_state = malloc_uncached(1024);
    assert(coeff && state && out && reference && saved_state);
    debugf("ULC_TEST_BEGIN text_bytes=%ld\n", (long)((uint8_t *)rsp_ulc.code_end - rsp_ulc.code));
    // 1024: direct full-block prefix/windowed paths and their mixed boundary.
    // <=512: reshuffle old-lap emission; 512 also emits decoded-block samples.
    for (int seed = 0; seed < 4; seed++) {
        for (int quiet = 0; quiet <= 1; quiet++) {
            for (int size = 128; size <= 1024; size *= 2) {
                const int overlaps[] = {0, 16, size / 2, size};
                for (int j = 0; j < 4; j++)
                    check_case(size, overlaps[j], size == 1024, quiet, seed);
            }
        }
        check_stereo();
    }
    printf("ULC: %d cases, %d failures\n", cases, failures);
    console_render();
    debugf("ULC_TEST_%s cases=%d failures=%d\n", failures ? "FAIL" : "PASS", cases, failures);
    for (;;) {}
}
