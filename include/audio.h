/**
 * @file audio.h
 * @brief Audio Subsystem
 * @ingroup audio
 */
#ifndef __LIBDRAGON_AUDIO_H
#define __LIBDRAGON_AUDIO_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @defgroup audio Audio Subsystem
 * @ingroup libdragon
 * @brief Interface to the N64 audio hardware.
 *
 * The audio subsystem handles queueing up chunks of audio data for
 * playback using the N64 audio DAC.  The audio subsystem handles
 * DMAing chunks of data to the audio DAC as well as audio callbacks
 * when there is room for another chunk to be written.  Buffer size
 * is calculated automatically based on the requested audio frequency.
 * The audio subsystem accomplishes this by interfacing with the audio
 * interface (AI) registers.
 *
 * Because the audio DAC is timed off of the system clock of the N64,
 * the audio subsystem needs to know what region the N64 is from.  This
 * is due to the fact that the system clock is timed differently for
 * PAL, NTSC and MPAL regions.  This is handled automatically by the
 * audio subsystem based on settings left by the bootloader.
 *
 * Code attempting to output audio on the N64 should initialize the
 * audio subsystem at the desired frequency and with the desired number
 * of buffers using #audio_init.  More audio buffers allows for smaller
 * chances of audio glitches but means that there will be more latency
 * in sound output.  When new data is available to be output, code should
 * check to see if there is room in the output buffers using
 * #audio_can_write.  Code can probe the current frequency and buffer
 * size using #audio_get_frequency and #audio_get_buffer_length respectively.
 * When there is additional room, code can add new data to the output
 * buffers using #audio_write.  Be careful as this is a blocking operation,
 * so if code doesn't check for adequate room first, this function will
 * not return until there is room and the samples have been written.
 * When all audio has been written, code should call #audio_close to shut
 * down the audio subsystem cleanly.
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Will be called periodically when more sample data is needed.
 *
 * @param[in] buffer        The address to write the sample data to
 * @param[in] numsamples    The number of samples to write to the buffer
 * 
 * NOTE: This is the number of samples per channel, so clients should write
 * twice this number of samples (interleaved).
 */
typedef void(*audio_fill_buffer_callback)(short *buffer, size_t numsamples);

/**
 * @brief Initialize the audio subsystem
 *
 * This function will set up the AI to play at a given frequency and
 * allocate a number of back buffers to write data to.
 *
 * @note Before re-initializing the audio subsystem to a new playback
 *       frequency, remember to call #audio_close.
 *
 * @param[in] frequency
 *            The frequency in Hz to play back samples at
 * @param[in] numbuffers
 *            The number of buffers to allocate internally
 */
void audio_init(const int frequency, int numbuffers);

/**
 * @brief Install a audio callback to fill the audio buffer when required.
 * 
 * This function allows to implement a pull-based audio system. It registers
 * a callback which will be invoked under interrupt whenever the AI is ready
 * to have more samples enqueued. The callback can fill the provided audio
 * data with samples that will be enqueued for DMA to AI.
 * 
 * @param[in] fill_buffer_callback   Callback to fill an empty audio buffer
 */
void audio_set_buffer_callback(audio_fill_buffer_callback fill_buffer_callback);

/**
 * @brief Pause or resume audio playback
 *
 * Should only be used when a fill_buffer_callback has been set
 * in #audio_init.
 * Silence will be generated while playback is paused.
 */
void audio_pause(bool pause);

/**
 * @brief Return whether there is an empty buffer to write to
 *
 * This function will check to see if there are any buffers that are not full to
 * write data to.  If all buffers are full, wait until the AI has played back
 * the next buffer in its queue and try writing again.
 */
volatile int audio_can_write();

/**
 * @brief Write a chunk of silence
 *
 * This function will write silence to be played back by the audio system.
 * It writes exactly #audio_get_buffer_length stereo samples.
 *
 * @note This function will block until there is room to write an audio sample.
 *       If you do not want to block, check to see if there is room by calling
 *       #audio_can_write.
 */
void audio_write_silence();

/**
 * @brief Close the audio subsystem
 *
 * This function closes the audio system and cleans up any internal
 * memory allocated by #audio_init.
 */
void audio_close();

/**
 * @brief Return actual frequency of audio playback
 *
 * @return Frequency in Hz of the audio playback
 */
int audio_get_frequency();

/**
 * @brief Get the number of stereo samples that fit into an allocated buffer
 *
 * @note To get the number of bytes to allocate, multiply the return by
 *       2 * sizeof( short )
 *
 * @return The number of stereo samples in an allocated buffer
 */
int audio_get_buffer_length();


/**
 * @brief Start writing to the first free internal buffer.
 * 
 * This function is similar to #audio_write but instead of taking samples
 * and copying them to an internal buffer, it returns the pointer to the
 * internal buffer. This allows generating the samples directly in the buffer
 * that will be sent via DMA to AI, without any subsequent memory copy.
 * 
 * The buffer should be filled with stereo interleaved samples, and
 * exactly #audio_get_buffer_length samples should be written.
 * 
 * After you have written the samples, call audio_write_end() to notify
 * the library that the buffer is ready to be sent to AI.
 * 
 * @note This function will block until there is room to write an audio sample.
 *       If you do not want to block, check to see if there is room by calling
 *       #audio_can_write.
 * 
 * @return  Pointer to the internal memory buffer where to write samples.
 */
short* audio_write_begin(void);

/**
 * @brief Complete writing to an internal buffer.
 * 
 * This function is meant to be used in pair with audio_write_begin().
 * Call this once you have generated the samples, so that the audio
 * system knows the buffer has been filled and can be played back.
 * 
 */
void audio_write_end(void);

/**
 * @brief Push a chunk of audio data (high-level function)
 * 
 * This function is an easy-to-use, higher level alternative to all
 * the audio_write* functions. It pushes audio samples into output
 * hiding the complexity required to match the fixed-size audio buffers.
 * 
 * The function accepts a @p buffer of stereo interleaved audio samples;
 * @p nsamples is the number of samples in the buffer. The function will
 * push the samples into output as much as possible.
 * 
 * If @p blocking is true, it will stop and wait until all samples have
 * been pushed into output. If @p blocking is false, it will stop as soon
 * as there are no more free buffers to push samples into, and will return
 * the number of pushed samples. It is up to the caller to then take care
 * of this and later try to call audio_push again with the remaining samples.
 * 
 * @note You CANNOT mixmatch this function with the other audio_write* functions,
 *       and viceversa. If you decide to use audio_push, use it exclusively to
 *       push the audio.
 * 
 * @param buffer        Buffer containing stereo samples to be played
 * @param nsamples      Number of stereo samples in the buffer
 * @param blocking      If true, wait until all samples have been pushed
 * @return int          Number of samples pushed into output
 */
int audio_push(const short *buffer, int nsamples, bool blocking);

__attribute__((deprecated("use audio_write_begin or audio_push instead")))
void audio_write(const short * const buffer);

#ifdef __cplusplus
}
#endif

/** @} */ /* display */

#endif
