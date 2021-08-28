/* Author: Romain "Artefact2" Dalmaso <artefact2@gmail.com> */
/* Contributor: Dan Spencer <dan@atomicpotato.net> */

/* This program is free software. It comes without any warranty, to the
 * extent permitted by applicable law. You can redistribute it and/or
 * modify it under the terms of the Do What The Fuck You Want To Public
 * License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details. */

#include "xm_internal.h"

/* .xm files are little-endian. */

/* Bounded reader macros.
 * If we attempt to read the buffer out-of-bounds, pretend that the buffer is
 * infinitely padded with zeroes.
 */
#define READ_U8_BOUND(offset, bound) (((offset) < bound) ? (*(uint8_t*)(moddata + (offset))) : 0)
#define READ_U16_BOUND(offset, bound) ((uint16_t)READ_U8(offset) | ((uint16_t)READ_U8((offset) + 1) << 8))
#define READ_U32_BOUND(offset, bound) ((uint32_t)READ_U16(offset) | ((uint32_t)READ_U16((offset) + 2) << 16))
#define READ_MEMCPY_BOUND(ptr, offset, length, bound) memcpy_pad(ptr, length, moddata, bound, offset)

#define READ_U8(offset) READ_U8_BOUND(offset, moddata_length)
#define READ_U16(offset) READ_U16_BOUND(offset, moddata_length)
#define READ_U32(offset) READ_U32_BOUND(offset, moddata_length)
#define READ_MEMCPY(ptr, offset, length) READ_MEMCPY_BOUND(ptr, offset, length, moddata_length)

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static inline void memcpy_pad(void* dst, size_t dst_len, const void* src, size_t src_len, size_t offset) {
	uint8_t* dst_c = dst;
	const uint8_t* src_c = src;

	/* how many bytes can be copied without overrunning `src` */
	size_t copy_bytes = (src_len >= offset) ? (src_len - offset) : 0;
	copy_bytes = copy_bytes > dst_len ? dst_len : copy_bytes;

	memcpy(dst_c, src_c + offset, copy_bytes);
	/* padded bytes */
	memset(dst_c + copy_bytes, 0, dst_len - copy_bytes);
}

int xm_check_sanity_preload(const char* module, size_t module_length) {
	if(module_length < 60) {
		return 4;
	}

	if(memcmp("Extended Module: ", module, 17) != 0) {
		return 1;
	}

	if(module[37] != 0x1A) {
		return 2;
	}

	if(module[59] != 0x01 || module[58] != 0x04) {
		/* Not XM 1.04 */
		return 3;
	}

	return 0;
}

int xm_check_sanity_postload(xm_context_t* ctx) {
	/* @todo: plenty of stuff to do here… */

	/* Check the POT */
	for(uint8_t i = 0; i < ctx->module.length; ++i) {
		if(ctx->module.pattern_table[i] >= ctx->module.num_patterns) {
			if(i+1 == ctx->module.length && ctx->module.length > 1) {
				/* Cheap fix */
				--ctx->module.length;
				DEBUG("trimming invalid POT at pos %X", i);
			} else {
				DEBUG("module has invalid POT, pos %X references nonexistent pattern %X",
				      i,
				      ctx->module.pattern_table[i]);
				return 1;
			}
		}
	}

	return 0;
}

size_t xm_get_memory_needed_for_context(const char* moddata, size_t moddata_length) {
	size_t memory_needed = 0;
	size_t offset = 60; /* Skip the first header */
	uint16_t num_channels;
	uint16_t num_patterns;
	uint16_t num_instruments;

	/* Read the module header */

	num_channels = READ_U16(offset + 8);
	num_channels = READ_U16(offset + 8);

	num_patterns = READ_U16(offset + 10);
	memory_needed += num_patterns * sizeof(xm_pattern_t);

	num_instruments = READ_U16(offset + 12);
	memory_needed += num_instruments * sizeof(xm_instrument_t);

	memory_needed += MAX_NUM_ROWS * READ_U16(offset + 4) * sizeof(uint8_t); /* Module length */

	/* Header size */
	offset += READ_U32(offset);

	/* Read pattern headers */
	for(uint16_t i = 0; i < num_patterns; ++i) {
		uint16_t num_rows;

		num_rows = READ_U16(offset + 5);
		memory_needed += num_rows * num_channels * sizeof(xm_pattern_slot_t);

		/* Pattern header length + packed pattern data size */
		offset += READ_U32(offset) + READ_U16(offset + 7);
	}

	/* Read instrument headers */
	for(uint16_t i = 0; i < num_instruments; ++i) {
		uint16_t num_samples;
		uint32_t sample_size_aggregate = 0;

		num_samples = READ_U16(offset + 27);
		memory_needed += num_samples * sizeof(xm_sample_t);

		/* Instrument header size */
		uint32_t ins_header_size = READ_U32(offset);
		if (ins_header_size == 0 || ins_header_size > INSTRUMENT_HEADER_LENGTH)
			ins_header_size = INSTRUMENT_HEADER_LENGTH;
		offset += ins_header_size;

		for(uint16_t j = 0; j < num_samples; ++j) {
			uint32_t sample_size;

			sample_size = READ_U32(offset);
			sample_size_aggregate += sample_size;
			memory_needed += sample_size;
			offset += 40; /* See comment in xm_load_module() */
		}

		offset += sample_size_aggregate;
	}

	memory_needed += num_channels * sizeof(xm_channel_context_t);
	memory_needed += sizeof(xm_context_t);

	return memory_needed;
}

char* xm_load_module(xm_context_t* ctx, const char* moddata, size_t moddata_length, char* mempool) {
	size_t offset = 0;
	xm_module_t* mod = &(ctx->module);

	/* Read XM header */
#if XM_STRINGS
	READ_MEMCPY(mod->name, offset + 17, MODULE_NAME_LENGTH);
	READ_MEMCPY(mod->trackername, offset + 38, TRACKER_NAME_LENGTH);
#endif
	offset += 60;

	/* Read module header */
	uint32_t header_size = READ_U32(offset);

	mod->length = READ_U16(offset + 4);
	mod->restart_position = READ_U16(offset + 6);
	mod->num_channels = READ_U16(offset + 8);
	mod->num_patterns = READ_U16(offset + 10);
	mod->num_instruments = READ_U16(offset + 12);

	mod->patterns = (xm_pattern_t*)mempool;
	mempool += mod->num_patterns * sizeof(xm_pattern_t);

	mod->instruments = (xm_instrument_t*)mempool;
	mempool += mod->num_instruments * sizeof(xm_instrument_t);

	uint16_t flags = READ_U32(offset + 14);
	mod->frequency_type = (flags & (1 << 0)) ? XM_LINEAR_FREQUENCIES : XM_AMIGA_FREQUENCIES;

	ctx->tempo = READ_U16(offset + 16);
	ctx->bpm = READ_U16(offset + 18);

	READ_MEMCPY(mod->pattern_table, offset + 20, PATTERN_ORDER_TABLE_LENGTH);
	offset += header_size;

	/* Read patterns */
	for(uint16_t i = 0; i < mod->num_patterns; ++i) {
		uint16_t packed_patterndata_size = READ_U16(offset + 7);
		xm_pattern_t* pat = mod->patterns + i;

		pat->num_rows = READ_U16(offset + 5);

		pat->slots = (xm_pattern_slot_t*)mempool;
		mempool += mod->num_channels * pat->num_rows * sizeof(xm_pattern_slot_t);

		/* Pattern header length */
		offset += READ_U32(offset);

		if(packed_patterndata_size == 0) {
			/* No pattern data is present */
			memset(pat->slots, 0, sizeof(xm_pattern_slot_t) * pat->num_rows * mod->num_channels);
		} else {
			/* This isn't your typical for loop */
			for(uint16_t j = 0, k = 0; j < packed_patterndata_size; ++k) {
				uint8_t note = READ_U8(offset + j);
				xm_pattern_slot_t* slot = pat->slots + k;

				if(note & (1 << 7)) {
					/* MSB is set, this is a compressed packet */
					++j;

					if(note & (1 << 0)) {
						/* Note follows */
						slot->note = READ_U8(offset + j);
						++j;
					} else {
						slot->note = 0;
					}

					if(note & (1 << 1)) {
						/* Instrument follows */
						slot->instrument = READ_U8(offset + j);
						++j;
					} else {
						slot->instrument = 0;
					}

					if(note & (1 << 2)) {
						/* Volume column follows */
						slot->volume_column = READ_U8(offset + j);
						++j;
					} else {
						slot->volume_column = 0;
					}

					if(note & (1 << 3)) {
						/* Effect follows */
						slot->effect_type = READ_U8(offset + j);
						++j;
					} else {
						slot->effect_type = 0;
					}

					if(note & (1 << 4)) {
						/* Effect parameter follows */
						slot->effect_param = READ_U8(offset + j);
						++j;
					} else {
						slot->effect_param = 0;
					}
				} else {
					/* Uncompressed packet */
					slot->note = note;
					slot->instrument = READ_U8(offset + j + 1);
					slot->volume_column = READ_U8(offset + j + 2);
					slot->effect_type = READ_U8(offset + j + 3);
					slot->effect_param = READ_U8(offset + j + 4);
					j += 5;
				}
			}
		}

		offset += packed_patterndata_size;
	}

	/* Read instruments */
	for(uint16_t i = 0; i < ctx->module.num_instruments; ++i) {
		xm_instrument_t* instr = mod->instruments + i;

		/* Original FT2 would load instruments with a direct read into the
		   instrument data structure that was previously zeroed. This means
		   that if the declared length was less than INSTRUMENT_HEADER_LENGTH,
		   all excess data would be zeroed. This is used by the XM compressor
		   BoobieSqueezer. To implement this, bound all reads to the header size. */
		uint32_t ins_header_size = READ_U32(offset);
		if (ins_header_size == 0 || ins_header_size > INSTRUMENT_HEADER_LENGTH)
			ins_header_size = INSTRUMENT_HEADER_LENGTH;

#if XM_STRINGS
		READ_MEMCPY_BOUND(instr->name, offset + 4, INSTRUMENT_NAME_LENGTH, offset + ins_header_size);
		instr->name[INSTRUMENT_NAME_LENGTH] = 0;
#endif
	    instr->num_samples = READ_U16_BOUND(offset + 27, offset + ins_header_size);

		if(instr->num_samples > 0) {
			/* Read extra header properties */
			READ_MEMCPY_BOUND(instr->sample_of_notes, offset + 33, NUM_NOTES, offset + ins_header_size);

			instr->volume_envelope.num_points = READ_U8_BOUND(offset + 225, offset + ins_header_size);
			if (instr->volume_envelope.num_points > NUM_ENVELOPE_POINTS)
				instr->volume_envelope.num_points = NUM_ENVELOPE_POINTS;

			instr->panning_envelope.num_points = READ_U8_BOUND(offset + 226, offset + ins_header_size);
			if (instr->panning_envelope.num_points > NUM_ENVELOPE_POINTS)
				instr->panning_envelope.num_points = NUM_ENVELOPE_POINTS;

			for(uint8_t j = 0; j < instr->volume_envelope.num_points; ++j) {
				instr->volume_envelope.points[j].frame = READ_U16_BOUND(offset + 129 + 4 * j, offset + ins_header_size);
				instr->volume_envelope.points[j].value = READ_U16_BOUND(offset + 129 + 4 * j + 2, offset + ins_header_size);
			}

			for(uint8_t j = 0; j < instr->panning_envelope.num_points; ++j) {
				instr->panning_envelope.points[j].frame = READ_U16_BOUND(offset + 177 + 4 * j, offset + ins_header_size);
				instr->panning_envelope.points[j].value = READ_U16_BOUND(offset + 177 + 4 * j + 2, offset + ins_header_size);
			}

			instr->volume_envelope.sustain_point = READ_U8_BOUND(offset + 227, offset + ins_header_size);
			instr->volume_envelope.loop_start_point = READ_U8_BOUND(offset + 228, offset + ins_header_size);
			instr->volume_envelope.loop_end_point = READ_U8_BOUND(offset + 229, offset + ins_header_size);

			instr->panning_envelope.sustain_point = READ_U8_BOUND(offset + 230, offset + ins_header_size);
			instr->panning_envelope.loop_start_point = READ_U8_BOUND(offset + 231, offset + ins_header_size);
			instr->panning_envelope.loop_end_point = READ_U8_BOUND(offset + 232, offset + ins_header_size);

			// Fix broken modules with loop points outside of defined points
			if (instr->volume_envelope.num_points > 0) {
				instr->volume_envelope.loop_start_point =
					MIN(instr->volume_envelope.loop_start_point, instr->volume_envelope.num_points-1);
				instr->volume_envelope.loop_end_point =
					MIN(instr->volume_envelope.loop_end_point,   instr->volume_envelope.num_points-1);
			}
			if (instr->panning_envelope.num_points > 0) {
				instr->panning_envelope.loop_start_point =
					MIN(instr->panning_envelope.loop_start_point, instr->panning_envelope.num_points-1);
				instr->panning_envelope.loop_end_point =
					MIN(instr->panning_envelope.loop_end_point,   instr->panning_envelope.num_points-1);
			}

			uint8_t flags = READ_U8_BOUND(offset + 233, offset + ins_header_size);
			instr->volume_envelope.enabled = flags & (1 << 0);
			instr->volume_envelope.sustain_enabled = flags & (1 << 1);
			instr->volume_envelope.loop_enabled = flags & (1 << 2);

			flags = READ_U8_BOUND(offset + 234, offset + ins_header_size);
			instr->panning_envelope.enabled = flags & (1 << 0);
			instr->panning_envelope.sustain_enabled = flags & (1 << 1);
			instr->panning_envelope.loop_enabled = flags & (1 << 2);

			instr->vibrato_type = READ_U8_BOUND(offset + 235, offset + ins_header_size);
			if(instr->vibrato_type == 2) {
				instr->vibrato_type = 1;
			} else if(instr->vibrato_type == 1) {
				instr->vibrato_type = 2;
			}
			instr->vibrato_sweep = READ_U8_BOUND(offset + 236, offset + ins_header_size);
			instr->vibrato_depth = READ_U8_BOUND(offset + 237, offset + ins_header_size);
			instr->vibrato_rate = READ_U8_BOUND(offset + 238, offset + ins_header_size);
			instr->volume_fadeout = READ_U16_BOUND(offset + 239, offset + ins_header_size);

			instr->samples = (xm_sample_t*)mempool;
			mempool += instr->num_samples * sizeof(xm_sample_t);
		} else {
			instr->samples = NULL;
		}

		/* Instrument header size */
		offset += ins_header_size;

		for(uint16_t j = 0; j < instr->num_samples; ++j) {
			/* Read sample header */
			xm_sample_t* sample = instr->samples + j;

			sample->length = READ_U32(offset);
			sample->loop_start = READ_U32(offset + 4);
			sample->loop_length = READ_U32(offset + 8);
			sample->loop_end = sample->loop_start + sample->loop_length;
			sample->volume = (float)READ_U8(offset + 12) / (float)0x40;
			sample->finetune = (int8_t)READ_U8(offset + 13);

			/* Fix invalid loop definitions */
			if (sample->loop_start > sample->length)
				sample->loop_start = sample->length;
			if (sample->loop_end > sample->length)
				sample->loop_end = sample->length;
			sample->loop_length = sample->loop_end - sample->loop_start;

			uint8_t flags = READ_U8(offset + 14);
			if((flags & 3) == 0 || sample->loop_length == 0) {
				sample->loop_type = XM_NO_LOOP;
			} else if((flags & 3) == 1) {
				sample->loop_type = XM_FORWARD_LOOP;
			} else {
				sample->loop_type = XM_PING_PONG_LOOP;
			}

			sample->bits = (flags & (1 << 4)) ? 16 : 8;

			sample->panning = (float)READ_U8(offset + 15) / (float)0xFF;
			sample->relative_note = (int8_t)READ_U8(offset + 16);
#if XM_STRINGS
			READ_MEMCPY(sample->name, offset + 18, SAMPLE_NAME_LENGTH);
			sample->name[SAMPLE_NAME_LENGTH] = 0;
#endif
			sample->data8 = (int8_t*)mempool;
			mempool += sample->length;

			if(sample->bits == 16) {
				sample->loop_start >>= 1;
				sample->loop_length >>= 1;
				sample->loop_end >>= 1;
				sample->length >>= 1;
			}

			/* Notice that, even if there's a "sample header size" in the
			   instrument header, that value seems ignored, and might even
			   be wrong in some corrupted modules. */
			offset += 40;
		}

		for(uint16_t j = 0; j < instr->num_samples; ++j) {
			/* Read sample data */
			xm_sample_t* sample = instr->samples + j;
			uint32_t length = sample->length;

			if(sample->bits == 16) {
				int16_t v = 0;
				for(uint32_t k = 0; k < length; ++k) {
					v = v + (int16_t)READ_U16(offset + (k << 1));
					sample->data16[k] = v;
				}
				offset += sample->length << 1;
			} else {
				int8_t v = 0;
				for(uint32_t k = 0; k < length; ++k) {
					v = v + (int8_t)READ_U8(offset + k);
					sample->data8[k] = v;
				}
				offset += sample->length;
			}
		}
	}

	return mempool;
}
