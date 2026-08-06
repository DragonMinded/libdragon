/**
 * @file test_samplebuffer.c
 * @brief Standalone testrom for the samplebuffer circular logic
 *
 * A synthetic producer appends fixed-size chunks whose content is a known
 * function of the absolute waveform position; every window handed out by
 * #samplebuffer_get is verified against it. Covers declared append
 * granularity, seeks, undos, and 8-bit odd-wrap parity.
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
	(void)ctx;
	int n = (wlen + SB_CHUNK - 1) / SB_CHUNK;
	int16_t *out = samplebuffer_append(sbuf, n * SB_CHUNK);
	for (int i = 0; i < n * SB_CHUNK; i++)
		out[i] = sb_expected(wpos + i);
	// Emulate the Opus intra-frame skip: part of the decoded frame is dropped.
	if (sb_undo_units && (seeking || sb_undo_always))
		samplebuffer_undo(sbuf, sb_undo_units);
}

static waveform_t sb_wave = {
	.name = "fake", .bits = 16, .channels = 1, .frequency = 48000,
	.len = 1 << 30, .read = sb_prod_read, .append_units = SB_CHUNK,
};

static waveform_t sb_rsp_wave = {
	.name = "fake-rsp", .bits = 16, .channels = 1, .frequency = 48000,
	.len = 1 << 30, .append_units = 1024, .rsp_written = true,
};

// ULC produces an aligned full block and undoes the invalid tail. Verify that
// reusing its stereo ring for mono keeps the next RSP destination aligned.
static bool sb_rsp_undo_alignment(void)
{
	void *mem = malloc_uncached(16384);
	assertf(mem != NULL, "malloc_uncached failed");

	samplebuffer_t sb = {0};
	samplebuffer_init(&sb, mem, 16384, 0);
	samplebuffer_set_bps(&sb, 32);
	samplebuffer_set_waveform(&sb, &sb_rsp_wave, NULL);

	// 42 stereo frames occupy 168 bytes (aligned), but carrying the numeric
	// head 42 into a mono configuration would address byte 84.
	samplebuffer_append(&sb, 1024);
	samplebuffer_undo(&sb, 1024 - 42);
	samplebuffer_flush(&sb);
	samplebuffer_set_bps(&sb, 16);
	samplebuffer_set_waveform(&sb, &sb_rsp_wave, NULL);
	void *mono = samplebuffer_append(&sb, 1024);
	bool ok = ((uintptr_t)mono & 7) == 0;

	char line[96];
	snprintf(line, sizeof(line), "  %-22s %s\n",
		"rsp undo/reconfigure", ok ? "PASS" : "FAIL");
	printf("%s", line);
	debugf("%s", line);

	memset(&sb, 0, sizeof(sb));
	free_uncached(mem);
	return ok;
}

// 8-bit PCM: unit_bytes is 1, so an odd-length append leaves widx odd and the
// next discard can make wpos odd. The ring slot and the waveform position must
// keep the same byte parity for dma_read; both becoming odd is fine.
static int8_t sb8_expected(int pos) {
	return (int8_t)(pos * 17 + 3);
}

static void sb8_prod_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking)
{
	(void)ctx; (void)seeking;
	// Odd chunks so the write cursor spends most of its life on an odd index.
	while (wlen > 0) {
		int n = wlen > 17 ? 17 : wlen;
		int8_t *out = samplebuffer_append(sbuf, n);
		for (int i = 0; i < n; i++)
			out[i] = sb8_expected(wpos + i);
		wlen -= n;
		wpos += n;
	}
}

static waveform_t sb8_wave = {
	.name = "fake8", .bits = 8, .channels = 1, .frequency = 22050,
	.len = 1 << 30, .read = sb8_prod_read,
};

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

static bool sb8_stress(const char *tag, int nbytes, int niter)
{
	sb_nwindows = 0;

	void *mem = malloc_uncached(nbytes);
	assertf(mem != NULL, "malloc_uncached(%d) failed", nbytes);

	samplebuffer_t sb = {0};
	samplebuffer_init(&sb, mem, nbytes, 0);
	samplebuffer_set_bps(&sb, 8);
	samplebuffer_set_waveform(&sb, &sb8_wave, sb8_prod_read);

	uint32_t rng = 424242;
	int pos = 0;
	bool ok = true;
	for (int i = 0; i < niter && ok; i++) {
		rng = rng * 1103515245 + 12345;
		int want = 1 + (int)((rng >> 9) % 100);
		int len = want;
		int8_t *p = samplebuffer_get(&sb, pos, &len);
		assertf(len > 0, "samplebuffer_get returned nothing");
		sb_nwindows++;
		for (int j = 0; j < len; j++) {
			if (p[j] != sb8_expected(pos + j)) {
				printf("  FAIL 8bit mismatch at pos=%d (+%d): got %02x, expected %02x\n",
					pos, j, (uint8_t)p[j], (uint8_t)sb8_expected(pos + j));
				ok = false;
				break;
			}
		}
		// Consume an odd count often enough that the next discard leaves an
		// odd wpos: that is the state the old assert rejected.
		pos += 1 + (int)((rng >> 17) % len);
	}

	char line[96];
	snprintf(line, sizeof(line), "  %-22s %s  size=%5d windows=%5d\n",
		tag, ok ? "PASS" : "FAIL", sb.size, sb_nwindows);
	printf("%s", line);
	debugf("%s", line);

	memset(&sb, 0, sizeof(sb));
	free_uncached(mem);
	return ok;
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
	samplebuffer_set_waveform(&sb, &sb_wave, sb_prod_read);

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

	// Reconfiguring a samplebuffer may call #rspq_highpri_sync; keep the queue
	// engine alive.
	rspq_init();

	printf("samplebuffer stress tests\n\n");
	debugf("samplebuffer stress tests\n\n");

	// Usable size covers several chunks plus the mirrored tail (sized to
	// append_units). `odd` is not a multiple of the chunk, so every lap of the
	// ring spills a different amount into the tail. Stress with seeks and
	// undos across the wrap.
	int big = (SB_CHUNK * 4 + SB_CHUNK) * 2;
	int odd = big + 2 * 133;
	int small = (SB_CHUNK * 3 + SB_CHUNK) * 2;

	int total = 0;
	#define RUN(...) do { total++; if (!sb_stress(__VA_ARGS__)) sb_failed++; } while (0)
	RUN("plain",       big,   3000, 0,   0,   false);
	RUN("seek",        big,   3000, 97,  0,   false);
	RUN("seek+undo",   big,   3000, 97,  333, false);
	RUN("odd",         odd,   3000, 0,   0,   false);
	RUN("odd+undo",    odd,   3000, 97,  333, false);
	RUN("drift",       small, 6000, 0,   333, true);
	RUN("drift+seek",  small, 6000, 397, 333, true);
	RUN("drift-odd",   odd,   6000, 0,   333, true);
	#undef RUN
	
	total++;
	if (!sb_rsp_undo_alignment())
		sb_failed++;

	// 8-bit PCM with odd appends: wraps the ring with an odd wpos.
	{
		int nbytes = (SAMPLEBUFFER_MARGIN_UNITS * 4 + SAMPLEBUFFER_MARGIN_UNITS) * 1;
		total++;
		if (!sb8_stress("pcm8/odd-wrap", nbytes, 8000))
			sb_failed++;
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
