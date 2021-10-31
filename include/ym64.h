#ifndef __LIBDRAGON_YM64_H
#define __LIBDRAGON_YM64_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "mixer.h"
#include "ay8910.h"

/// @cond
typedef struct _LHANewDecoder LHANewDecoder;
/// @endcond

/**
 * @file ym64.h
 * @brief Player for the .YM64 module format (Arkos Tracker 2)
 * @ingroup mixer
 *
 * ym64player_t is a player of the .YM64 file format, which is based on the
 * .YM module format, a format first popularized in the Atari ST emulator scene.
 * 
 * The format is based around the very popular AY-3-8910 sound chip, that
 * was powering a few successful 8-bit consoles such as Atari ST, ZX Spectrum
 * and Amstrad CPC. It is a 3-channel PSG with envelope support. It can produce
 * typical 8-bit "chiptune" music scores. Nowadays, it is possible to compose
 * soundtracks using the Arkos Tracker 2 tool, that exports in the YM format.
 * 
 * The YM format is a simple dump of the state of all registers of the AY
 * chip at a fixed time step. To playback, it is necessary to emulate the
 * AY PSG. The implementation has been carefully optimized for the N64 MIPS
 * CPU for high-performance, so that playback typically takes less than 5%
 * of CPU time, plus a few percents of RSP time for resampling and mixing
 * (done by the mixer).
 * 
 * The YM64 is actually a valid YM file that has been simply normalized against
 * the different existing revisions, in a way to be efficient for reproduction
 * on N64. audioconv64 can convert YM to YM64 (or leave them as-is if they
 * already fully compatible).
 * 
 * The main conversion option to pay attention too is whether the output file
 * must be compressed or not. Compressed files are smaller but takes 18Kb
 * more of RDRAM to be played back and cannot be seeked.
 * 
 * This player is dedicated to the late Sir Clive Sinclair whose computer,
 * powered by the AY-3-8910, helped popularize what we now call
 * chiptune music. -- Rasky
 *
 */

/**
 * @brief Player of a .YM64 file. 
 * 
 * This structure holds the state a player of a YM64 module. It can be
 * initialized using #ym64player_open, and played with #ym64player_play.
 * 
 * See the rest of this module for more functions.
 */
typedef struct {
	waveform_t wave;          ///< waveform for playback with the mixer

	FILE *f;                  ///< Open file handle
	LHANewDecoder *decoder;   ///< Optional LHA decoder (compressed YM files)
	int start_off;            ///< Starting offset of the first audio frame

	AY8910 ay;                ///< AY8910 emulator
	uint8_t regs[16];         ///< Current cached value of the AY registers
	uint32_t nframes;         ///< Number of YM audio frames
	uint32_t chipfreq;        ///< Operating frequency of the AY chip
	uint16_t playfreq;        ///< Frequency of an audio frame (typically 50Hz or 60Hz)
	int curframe;             ///< Current audio frame being played

	int first_ch;             ///< First channel used in the mixer for playback
} ym64player_t;

/** @brief Structure containing information about a YM song */
typedef struct {
	char name[128];           ///< Name of the song
	char author[128];         ///< Author of the song
	char comment[128];        ///< Comment of the song
} ym64player_songinfo_t;

/**
 * @brief Open a YM64 file for playback
 * 
 * @param[in]  	player 		YM64 player to initialize
 * @param[in] 	fn 			Filename of the XM64 (with filesystem prefix, e.g. `rom://`).
 * @param[out] 	info 		Optional structure to fill with information on the song
 *                          (pass NULL if not needed)
 */
void ym64player_open(ym64player_t *player, const char *fn, ym64player_songinfo_t *info);

/**
 * @brief Return the number of channels used in the mixer for playback.
 * 
 * Depending on the AY emulator compile-time settings, this could be either
 * 1 or 2 (mono or stereo). Notice that the YM64 currently mixes itself the
 * 3 internal channels of the AY8910 chip, so only a final output stream
 * is passed to the mixer.
 * 
 * @param[in]  	player 		YM64 player
 * @return  				Number of mixer channels.
 */
int ym64player_num_channels(ym64player_t *player);

/**
 * @brief Start playback of a YM file.
 * 
 * @param[in]	player 		YM64 player
 * @param[in] 	first_ch	First mixer channel to use for playback
 */
void ym64player_play(ym64player_t *player, int first_ch);

/**
 * @brief Read the total duration the YM module.
 * 
 * The function returns the total duration of the YM module, in ticks (internal
 * YM position) or seconds. You can pass NULL to information that you are not
 * interested in receiving.
 * 
 * @param[in]	player 		YM64 player
 * @param[out] 	len 		Total duration in ticks
 * @param[out] 	secs 		Total duration in seconds
 */
void ym64player_duration(ym64player_t *player, int *len, float *secs);

/**
 * @brief Read the current position of the YM module.
 * 
 * The function returns the current position expressed in ticks (internal
 * YM position), and also expressed as number of seconds. You can pass NULL
 * to information that you are not interested in receiving.
 * 
 * @param[in]	player 		YM64 player
 * @param[out] 	pos 		Current position in ticks
 * @param[out] 	secs 		Current position in seconds
 */
void ym64player_tell(ym64player_t *player, int *pos, float *secs);

/**
 * @brief Seek to a specific position in the YM module.
 * 
 * The function seeks to a new absolute position expressed in ticks (internal
 * YM position). Notice that it's not possible to seek in a YM64 file that has
 * been compressed. audioconv64 compresses YM files by default.
 * 
 * @param[in]	player 		YM64 player
 * @param[out] 	pos 		Absolute position in ticks
 * @return                  True if it was possible to seek, false if 
 *                          the file is compressed.
 */
bool ym64player_seek(ym64player_t *player, int pos);

/**
 * @brief Stop YM playback.
 * 
 * The YM module will keep the current position. Use #ym64player_play to continue
 * playback.
 */
void ym64player_stop(ym64player_t *player);

/**
 * @brief Close and deallocate the YM64 player.
 */
void ym64player_close(ym64player_t *player);

#ifdef __cplusplus
}
#endif

#endif
