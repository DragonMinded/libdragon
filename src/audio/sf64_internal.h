/**
 * @file sf64_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief On-disk layout and runtime state of the SF64 sound bank format
 * @ingroup mixer
 *
 * SF64 is the host-converted form of a SoundFont 2 bank: resolved presets and
 * regions plus embedded WAV64 samples. See tools/audioconv64/conv_sf64.cpp.
 */
#ifndef LIBDRAGON_SF64_INTERNAL_H
#define LIBDRAGON_SF64_INTERNAL_H

#include <stdint.h>

///@cond
typedef struct wav64_s wav64_t;
///@endcond

#define SF64_ID              "SF64"		///< SF64 file identifier

/** Loop modes for #sf64_region_t::loop_mode (SF2 SampleModes). */
#define SF64_LOOP_NONE       0			///< No loop
#define SF64_LOOP_CONTINUOUS 1			///< Continuous loop
#define SF64_LOOP_SUSTAIN    2			///< Sustain loop

/** @brief Amp envelope in SF2 units (timecents / centibels). */
typedef struct __attribute__((packed)) {
	int16_t delay_timecents;    ///< Delay before attack (timecents)
	int16_t attack_timecents;   ///< Attack duration (timecents)
	int16_t hold_timecents;     ///< Hold duration (timecents)
	int16_t decay_timecents;    ///< Decay duration (timecents)
	int16_t sustain_centibels;  ///< Sustain level (centibels below peak)
	int16_t release_timecents;  ///< Release duration (timecents)
	int16_t keynum_to_hold;     ///< Key-number scaling of hold (timecents/key)
	int16_t keynum_to_decay;    ///< Key-number scaling of decay (timecents/key)
} sf64_envelope_t;

/** @brief File header (big-endian on disk). */
typedef struct __attribute__((packed)) {
	char magic[4];              ///< Magic (#SF64_ID)
	uint8_t version;            ///< Format version
	uint8_t flags;              ///< Reserved flags
	uint16_t num_presets;       ///< Number of presets
	uint16_t num_regions;       ///< Number of regions
	uint16_t num_samples;       ///< Number of embedded samples
	uint32_t metadata_offset;   ///< Offset of asset-compressed metadata
	uint32_t metadata_size;     ///< Uncompressed metadata size
	uint32_t sample_data_offset;///< Offset of the first embedded WAV64
} sf64_header_t;

/** @brief MIDI preset → contiguous region range. */
typedef struct __attribute__((packed)) {
	uint16_t bank;              ///< MIDI bank number
	uint8_t program;            ///< MIDI program number
	uint8_t flags;              ///< Reserved flags
	uint16_t first_region;      ///< Index of the first region in the region table
	uint16_t num_regions;       ///< Number of contiguous regions
	uint32_t name_offset;       ///< Offset of the preset name in uncompressed metadata
} sf64_preset_t;

/** @brief Resolved instrument zone (key/vel filtered). */
typedef struct __attribute__((packed)) {
	uint16_t sample_index;      ///< Index into the sample table
	uint8_t key_min;            ///< Lowest MIDI key (inclusive)
	uint8_t key_max;            ///< Highest MIDI key (inclusive)
	uint8_t velocity_min;       ///< Lowest velocity (inclusive)
	uint8_t velocity_max;       ///< Highest velocity (inclusive)
	uint8_t loop_mode;          ///< Loop mode (#SF64_LOOP_NONE / CONTINUOUS / SUSTAIN)
	uint8_t exclusive_group;    ///< Exclusive class (0 = none)
	int8_t root_key;            ///< Root key / overriding root key
	int8_t coarse_tune;         ///< Coarse tune in semitones
	int16_t fine_tune;          ///< Fine tune in cents
	int16_t pitch_keytrack;     ///< Pitch key tracking (cents per key, SF2 ScaleTuning)
	int16_t attenuation_cb;     ///< Initial attenuation (centibels)
	int16_t pan;                ///< Pan in SF2 units (−500…500)
	sf64_envelope_t amp_env;    ///< Amplitude envelope
	uint16_t reserved_flags;    ///< Reserved flags for future generators
	uint16_t reserved[4];       ///< Reserved for future generators
} sf64_region_t;

/** @brief Embedded WAV64 sample variant. */
typedef struct __attribute__((packed)) {
	uint32_t wav64_offset;      ///< Absolute file offset of the WAV64 blob
	uint32_t wav64_size;        ///< Size of the WAV64 blob in bytes
	uint32_t pcm_hash;          ///< Hash of the PCM variant (for deduplication)
	uint32_t sample_start;      ///< Start sample (always 0 in v1; WAV64 already cropped)
	uint32_t sample_end;        ///< End sample (exclusive length of the waveform)
	uint32_t loop_start;        ///< Inclusive loop start (0 if no loop)
	uint32_t loop_end;          ///< Exclusive loop end (0 if no loop)
	uint32_t sample_rate;       ///< Native sample rate in Hz
	uint8_t channels;           ///< Channel count (1 = mono)
	uint8_t flags;              ///< Reserved flags
	uint16_t reserved;          ///< Reserved
} sf64_sample_t;

/**
 * @brief Loaded SF64 bank (opaque to API users).
 *
 * Immutable after #sf64_load. Safe to share across synthesizers.
 */
struct sf64_bank_s {
	int fd;                     ///< Open SF64 file (shared by embedded WAV64s)
	uint16_t num_presets;       ///< Number of presets
	uint16_t num_regions;       ///< Number of regions
	uint16_t num_samples;       ///< Number of embedded samples
	sf64_preset_t *presets;     ///< Preset table (into #meta)
	sf64_region_t *regions;     ///< Region table (into #meta)
	sf64_sample_t *samples;     ///< Sample table (into #meta)
	void *meta;                 ///< Decompressed metadata blob
	wav64_t *waves[];           ///< Embedded waveforms (#num_samples)
};

#ifdef __cplusplus
static_assert(sizeof(sf64_header_t) == 24, "invalid sf64_header size");
static_assert(sizeof(sf64_preset_t) == 12, "invalid sf64_preset size");
static_assert(sizeof(sf64_envelope_t) == 16, "invalid sf64_envelope size");
static_assert(sizeof(sf64_region_t) == 44, "invalid sf64_region size");
static_assert(sizeof(sf64_sample_t) == 36, "invalid sf64_sample size");
#else
_Static_assert(sizeof(sf64_header_t) == 24, "invalid sf64_header size");
_Static_assert(sizeof(sf64_preset_t) == 12, "invalid sf64_preset size");
_Static_assert(sizeof(sf64_envelope_t) == 16, "invalid sf64_envelope size");
_Static_assert(sizeof(sf64_region_t) == 44, "invalid sf64_region size");
_Static_assert(sizeof(sf64_sample_t) == 36, "invalid sf64_sample size");
#endif

#endif
