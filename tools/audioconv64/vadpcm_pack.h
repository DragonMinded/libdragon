// Native sub-nibble packing for VADPCM frames.
//
// libvadpcm always emits the classic 9-byte frame: one control byte followed
// by 16 residuals packed as nibbles. When the encoder is restricted to a
// smaller residual range (audioconv64 "bits=" option), those nibbles waste
// 1 or 2 bits each, so the frames are repacked to the dense layout the mixer
// ucode understands:
//
//     residual i occupies payload bits [i*bits, i*bits + bits)
//
// counting payload bit 0 as the most significant bit of payload byte 0. For
// bits==4 this is exactly the input layout, so the repack is the identity.

#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>

// Bytes of a VADPCM frame with `bits`-wide residuals (16 samples per frame).
static inline int vadpcm_frame_bytes(int bits) { return 2*bits + 1; }

// Read residual `i` of a 9-byte frame, sign-extended.
static inline int vadpcm_get_nibble(const uint8_t *frame, int i) {
	int r = (frame[1 + i/2] >> ((i & 1) ? 0 : 4)) & 0xF;
	return (r ^ 8) - 8;
}

// Repack one 9-byte frame into `vadpcm_frame_bytes(bits)` bytes.
static inline void vadpcm_pack_frame(uint8_t *dst, const uint8_t *src, int bits) {
	assert(bits >= 2 && bits <= 4);
	dst[0] = src[0];
	memset(dst + 1, 0, 2*bits);
	for (int i = 0; i < 16; i++) {
		int r = vadpcm_get_nibble(src, i);
		assert(r >= -(1 << (bits-1)) && r < (1 << (bits-1)));
		for (int b = 0; b < bits; b++) {
			int p = i*bits + b;
			dst[1 + p/8] |= ((r >> (bits-1-b)) & 1) << (7 - (p & 7));
		}
	}
}

// Inverse of vadpcm_pack_frame, used to validate the repack.
static inline void vadpcm_unpack_frame(uint8_t *dst, const uint8_t *src, int bits) {
	dst[0] = src[0];
	memset(dst + 1, 0, 8);
	for (int i = 0; i < 16; i++) {
		int r = 0;
		for (int b = 0; b < bits; b++) {
			int p = i*bits + b;
			r = (r << 1) | ((src[1 + p/8] >> (7 - (p & 7))) & 1);
		}
		r = (r ^ (1 << (bits-1))) - (1 << (bits-1));
		dst[1 + i/2] |= (r & 0xF) << ((i & 1) ? 0 : 4);
	}
}

// Repack a whole buffer of `nframes` frames. Returns the output size.
static inline int vadpcm_pack_frames(uint8_t *dst, const uint8_t *src, int nframes, int bits) {
	int fb = vadpcm_frame_bytes(bits);
	for (int i = 0; i < nframes; i++) {
		vadpcm_pack_frame(dst + i*fb, src + i*9, bits);
		uint8_t check[9];
		vadpcm_unpack_frame(check, dst + i*fb, bits);
		assert(memcmp(check, src + i*9, 9) == 0);
	}
	return nframes * fb;
}
