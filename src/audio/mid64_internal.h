/**
 * @file mid64_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief On-disk layout of the MID64 sequence format
 * @ingroup mixer
 *
 * See tools/audioconv64/conv_mid64.cpp for the encoder.
 */
#ifndef LIBDRAGON_MID64_INTERNAL_H
#define LIBDRAGON_MID64_INTERNAL_H

#include <stdint.h>

#define MID64_ID             "MD64"		///< MID64 file identifier
#define MID64_VERSION        1			///< Current format version
#define MID64_HEADER_SIZE    32			///< Size of #mid64_header_t
#define MID64_DEFAULT_TEMPO  500000		///< Default μs/quarter (120 BPM)

#define MID64_OP_SET_TEMPO       0xF0	///< Set tempo (3-byte μs/quarter)
#define MID64_OP_GM1_SYSTEM_ON   0xF1	///< General MIDI Level 1 System On
#define MID64_OP_END             0xFF	///< End of sequence

/** @brief File header (big-endian on disk; native on N64). */
typedef struct __attribute__((packed)) {
	char magic[4];				///< Magic (#MID64_ID)
	uint8_t version;			///< Format version (#MID64_VERSION)
	uint8_t flags;				///< Reserved flags
	uint16_t ppqn;				///< Pulses per quarter note
	uint32_t events_offset;		///< Byte offset of the event stream
	uint32_t events_size;		///< Size of the event stream in bytes
	uint32_t num_events;		///< Number of events including END
	uint32_t duration_ticks;	///< Sequence length in ticks
	uint32_t duration_ms;		///< Wall-clock length (tempo map), milliseconds
	uint32_t reserved;			///< Reserved
} mid64_header_t;

_Static_assert(sizeof(mid64_header_t) == MID64_HEADER_SIZE, "mid64_header_t size");

#endif
