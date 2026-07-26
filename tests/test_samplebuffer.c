/**
 * @file test_samplebuffer.c
 * @brief Standalone testrom for the samplebuffer circular logic
 *
 * A synthetic producer appends fixed-size chunks whose content is a known
 * function of the absolute waveform position; every window handed out by
 * #samplebuffer_get is verified against it. Covers declared /
 * auto-detected / small-declared append granularities, seeks and undos that
 * break the write phase (forcing an aligned relocate).
 */
#include <libdragon.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#define SB_CHUNK       960     // append granularity of the fake codec

static int sb_undo_units;      // units the producer drops from a read
static bool sb_undo_always;    // drop on every read, not just the seeking ones
static int sb_nwindows;
static int sb_failed;

static int16_t sb_expected(int pos) {
	return (int16_t)(pos * 7919 + 12345);
}

static void sb_prod_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking)
{
	int n = (wlen + SB_CHUNK - 1) / SB_CHUNK;
	int16_t *out = samplebuffer_append(sbuf, n * SB_CHUNK);
	for (int i = 0; i < n * SB_CHUNK; i++)
		out[i] = sb_expected(wpos + i);
	// Emulate the Opus intra-frame skip: part of the decoded frame is dropped,
	// which leaves the write cursor out of phase with the chunk size.
	if (sb_undo_units && (seeking || sb_undo_always))
		samplebuffer_undo(sbuf, sb_undo_units);
}

static waveform_t sb_wave = {
	.name = "fake", .bits = 16, .channels = 1, .frequency = 48000,
	.len = 1 << 30, .read = sb_prod_read, .append_units = SB_CHUNK,
};

// Same producer, but without declaring the granularity: the samplebuffer has
// to learn it from the appends themselves.
static waveform_t sb_wave_undeclared = {
	.name = "fake2", .bits = 16, .channels = 1, .frequency = 48000,
	.len = 1 << 30, .read = sb_prod_read,
};

// Declares a divisor of the chunk it really appends, small enough to fit the
// tail margin: useless as it is, so the samplebuffer must look past it.
static waveform_t sb_wave_small = {
	.name = "fake3", .bits = 16, .channels = 1, .frequency = 48000,
	.len = 1 << 30, .read = sb_prod_read, .append_units = 96,
};

static waveform_t *sb_cur_wave = &sb_wave;

static bool sb_check_window(const int16_t *p, int pos, int len)
{
	sb_nwindows++;
	for (int i = 0; i < len; i++) {
		if (p[i] != sb_expected(pos + i)) {
			printf("  FAIL mismatch at pos=%d (+%d): got %04x, expected %04x\n",
				pos, i, (uint16_t)p[i], (uint16_t)sb_expected(pos + i));
			debugf("  FAIL mismatch at pos=%d (+%d): got %04x, expected %04x\n",
				pos, i, (uint16_t)p[i], (uint16_t)sb_expected(pos + i));
			return false;
		}
	}
	return true;
}

// Drive a samplebuffer of `nbytes` for `niter` windows, seeking every
// `seek_every` iterations, and verify all the data handed out.
static bool sb_stress(const char *tag, int nbytes,
	int niter, int seek_every, int undo, bool ualways)
{
	sb_undo_units = undo;
	sb_undo_always = ualways;
	sb_nwindows = 0;

	void *mem = malloc_uncached(nbytes);
	assertf(mem != NULL, "malloc_uncached(%d) failed", nbytes);

	samplebuffer_t sb = {0};
	samplebuffer_init(&sb, mem, nbytes, 0);
	samplebuffer_set_bps(&sb, 16);
	samplebuffer_set_waveform(&sb, sb_cur_wave, sb_prod_read);

	uint32_t rng = 12345;
	int pos = 0;
	bool ok = true;
	for (int i = 0; i < niter && ok; i++) {
		rng = rng * 1103515245 + 12345;
		if (seek_every && i && i % seek_every == 0) {
			// Jump around: backwards (flush) or far forward (flush).
			pos = (int)((rng >> 8) % 100000);
			continue;
		}
		int want = 1 + (int)((rng >> 9) % 100);
		int len = want;
		int16_t *p = samplebuffer_get(&sb, pos, &len);
		assertf(len > 0, "samplebuffer_get returned nothing");
		ok = sb_check_window(p, pos, len);
		// Consume part of the window, like the mixer does.
		pos += 1 + (int)((rng >> 17) % len);
	}

	char line[96];
	snprintf(line, sizeof(line), "  %-22s %s  size=%5d windows=%5d\n",
		tag, ok ? "PASS" : "FAIL", sb.size, sb_nwindows);
	printf("%s", line);
	debugf("%s", line);

	// Do not call samplebuffer_close: it would free `mem` a second time.
	memset(&sb, 0, sizeof(sb));
	free_uncached(mem);
	return ok;
}

int main(void)
{
	debug_init_emulog();
	debug_init_usblog();
	emux_ioctl_fast();
	console_init();

	// Relocate may call #rspq_highpri_sync; keep the queue engine alive.
	rspq_init();

	printf("samplebuffer stress tests\n\n");
	debugf("samplebuffer stress tests\n\n");

	// A buffer whose usable size is a multiple of the chunk (the alignment
	// engages) and one where it is not (falls back to relocating), stressed
	// with seeks and with undos that break the write cursor phase.
	int big = (SB_CHUNK * 4 + SAMPLEBUFFER_MARGIN_UNITS) * 2;
	int odd = big + 2 * 133;
	int small = (SB_CHUNK * 3 + SAMPLEBUFFER_MARGIN_UNITS) * 2;

	static waveform_t *waves[] = { &sb_wave, &sb_wave_undeclared, &sb_wave_small };
	static const char *names[] = { "declared", "learnt", "smalldecl" };

	int total = 0;
	for (int w = 0; w < 3; w++) {
		sb_cur_wave = waves[w];
		char tag[48];
		#define T(name)  (sprintf(tag, "%s/%s", name, names[w]), tag)
		#define RUN(...) do { total++; if (!sb_stress(__VA_ARGS__)) sb_failed++; } while (0)
		RUN(T("plain"),       big,   3000, 0,   0,   false);
		RUN(T("seek"),        big,   3000, 97,  0,   false);
		RUN(T("seek+undo"),   big,   3000, 97,  333, false);
		RUN(T("odd"),         odd,   3000, 0,   0,   false);
		RUN(T("odd+undo"),    odd,   3000, 97,  333, false);
		RUN(T("drift"),       small, 6000, 0,   333, true);
		RUN(T("drift+seek"),  small, 6000, 397, 333, true);
		RUN(T("drift-odd"),   odd,   6000, 0,   333, true);
		#undef RUN
		#undef T
	}

	if (sb_failed) {
		printf("\n%d/%d TESTS FAILED\n", sb_failed, total);
		debugf("\n%d/%d TESTS FAILED\n", sb_failed, total);
		abort();
	}
	printf("\nALL TESTS PASSED (%d)\n", total);
	debugf("\nALL TESTS PASSED (%d)\n", total);
	return 0;
}
