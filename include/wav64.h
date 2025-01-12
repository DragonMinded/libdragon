/**
 * @file wav64.h
 * @brief Support for WAV64 audio files
 * @ingroup mixer
 */

#ifndef __LIBDRAGON_WAV64_H
#define __LIBDRAGON_WAV64_H

#include "mixer.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @cond
extern void __wav64_init_compression_lvl3(void);
/// @endcond

/** 
 * @brief WAV64 structure
 * 
 * This structure is initialized by #wav64_open to refer to an opened WAV64
 * file. It is meant to be played back through the audio mixer, implementing
 * the #waveform_t interface. As such, samples are not preloaded in memory
 * but rather loaded on request when needed for playback, streaming directly
 * from ROM. See #waveform_t for more details.
 * 
 * Use #wav64_play to playback. For more advanced usage, call directly the
 * mixer functions, accessing the #wave structure field.
 */
typedef struct wav64_s {
	/** @brief #waveform_t for this WAV64. 
	 * 
	 * Access and use this field directly with the mixer, if needed.
	 */
	waveform_t wave;

	/** @brief File descriptor to read WAV64 */
	int current_fd;			 ///< File descriptor for the wav64 file
	int base_offset;		 ///< Start of Wav64 data.
	int format;			     ///< Internal format of the file
	void *ext;               ///< Pointer to extended data (internal use)
} wav64_t;

/**
 * @brief Enable a non-default compression level
 * 
 * This function must be called if any wav64 that will be loaded use
 * a non-default compression level. The default compression level is 1 (VADPCM)
 * for which no initialization is required. Level 0 (uncompressed) also
 * requires no initialization.
 * 
 * Currently, only level 3 requires initialization (level 2 does not exist yet).
 * If you have any wav64 compressed with level 3, you must call this function
 * before opening them.
 * 
 * @code{.c}
 *      wav64_init_compression(3); 
 * 
 *      wav64_open(&jingle, "rom:/jingle.wav64");
 * @endcode
 * 
 * @param level     Compression level to initialize
 * 
 * @see #wav64_open
 * @hideinitializer
 */
#define wav64_init_compression(level) ({ \
    switch (level) { \
    case 0: break; \
    case 1: break; \
    case 3: __wav64_init_compression_lvl3(); break; \
    default: assertf(0, "Unsupported compression level: %d", level); \
    } \
})

/** @brief Open a WAV64 file for playback.
 * 
 * This function opens the file, parses the header, and initializes for
 * playing back through the audio mixer.
 * 
 * @param   wav         Pointer to wav64_t structure
 * @param   fn          Filename of the wav64 (with filesystem prefix). Currently,
 *                      only files on DFS ("rom:/") are supported.
 */ 
void wav64_open(wav64_t *wav, const char *fn);

/** @brief Configure a WAV64 file for looping playback. */
void wav64_set_loop(wav64_t *wav, bool loop);

/** @brief Start playing a WAV64 file.
 * 
 * This is just a simple wrapper that calls #mixer_ch_play on the WAV64's
 * waveform (wav64_t::wave). For advanced usages, please call directly the
 * mixer functions.
 * 
 * It is possible to start the same waveform on multiple independent channels.
 * Playback will automatically stop when the waveform is finished, unless it
 * is looping. To stop playing a wav64 file before it is normally finished,
 * call #mixer_ch_stop on the channel used for playback. 
 * 
 * @param   wav         Pointer to wav64_t structure
 * @param   ch          Channel of the mixer to use for playback.
 */
void wav64_play(wav64_t *wav, int ch);

/**
 * @brief Get the (possibly compressed) bitrate of the WAV64 file.
 * 
 * @param wav 			Pointer to wav64_t structure
 * @return int 			Bitrate in bits per second
 */
int wav64_get_bitrate(wav64_t *wav);

/**
 * @brief Close a WAV64 file.
 * 
 * This function closes the file and frees any resources associated with it.
 * If the file is currently playing, playback will be stopped.
 * 
 * @param wav 			Pointer to wav64_t structure
 */
void wav64_close(wav64_t *wav);

#ifdef __cplusplus
}
#endif

#endif
