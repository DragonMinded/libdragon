/**
 * @file mixer.h
 * @brief RSP Audio mixer 
 * @ingroup mixer
 */

#ifndef __LIBDRAGON_MIXER_H
#define __LIBDRAGON_MIXER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup mixer Audio mixer
 * @ingroup audio
 * @brief Flexible, composable, fast, RSP-based audio mixer.
 *
 * This module offers a flexible API to mix and play up to 32 independent audio
 * streams called "waveforms". It also supports resampling: each waveform can
 * play at a different playback frequency, which in turn can be different from
 * the final output frequency. The resampling and mixing is performed by a very
 * efficient RSP microcode (see rsp_mixer.S).
 *
 * The mixer exposes 32 channels that can be used to play different audio sources.
 * An audio source is called a "waveform", and is represented by the type
 * waveform_t. To be able to produce audio that can be mixed (eg: decompress
 * and playback a MP3 file), the decoder/player code must implement a waveform_t.
 *
 * waveform_t can be thought of as a "sample generator". The mixer does not force
 * the whole audio stream to be decompressed in memory before playing, but rather
 * work using callbacks: whenever new samples are required, a specific callback
 * (defined in the waveform_t instance) is called, and the callback is expected
 * to decompress and store a chunk of samples into a buffer provided by the mixer.
 * This allows full decoupling between the mixer and the different audio players.
 * Audio players are free to generate audio samples as they wish (eg: via RSP),
 * as long as they honor the waveform_t protocol. See waveform_t for more
 * information.
 *
 * One of the main design goals for the mixer is to provide an efficient way to
 * compose different audio sources. To achieve full flexibility without impacting
 * speed, the mixer tries to honor the "CPU zero copy" design principle: samples are
 * (almost) never touched or moved around with CPU. The whole API has been
 * carefully designed so that audio players can produce samples directly into
 * a mixer-provided buffer, so that the mixer can then resample and mix them on
 * the RSP and store the final output stream directly in the final buffer which
 * is sent via DMA to the N64 AI hardware DAC. In general, the N64 CPU is very
 * bandwidth-limited on RDRAM accesses, so "touching" 44100 samples per second
 * with the CPU would already consume too many resources. Audio players are
 * expected to use RSP to produce the audio samples as well, so that the full
 * audio pipeline can work totally off CPU.
 */

/** @brief Maximum number of channels supported by the mixer */
#define MIXER_MAX_CHANNELS      32

/**
 * Number of bytes in sample buffers that must be over-read to make the
 * RSP ucode safe.
 * 
 * RSP ucode doesn't currently bound check sample buffer accesses for
 * performance reasons (and missing implementation). In case of loops,
 * this means that the RSP will go beyond the loop end point, before
 * looping, up to 64 bytes (which is the internal DMEM buffer, called
 * DMEM_SAMPLE_CACHE).
 * 
 * So in general, when playing a looping waveform, the mixer will need
 * to repeat the loop start after the loop end for up to 64 bytes.
 */
#define MIXER_LOOP_OVERREAD     64

/// @cond
typedef struct waveform_s waveform_t;
typedef struct samplebuffer_s samplebuffer_t;
/// @endcond

/**
 * @brief Initialize the mixer
 * 
 * The mixer must be initialized after the audio subsystem (audio_init).
 * The number of channels specified is the maximum number of channels
 * used by the application. Specifying a higher number means using
 * more memory as the mixer will allocate one sample buffer per channel,
 * but it does not affect performance (which correlates to the
 * actual number of simultaneously playing channels).
 * 
 * @param[in]    num_channels   Number of channels to initialize.
 */
void mixer_init(int num_channels);

/**
 * @brief Deinitialize the mixer.
 */
void mixer_close(void);

/**
 * @brief Set master volume. 
 * 
 * This is a global attenuation factor (range [0..1]) that will be applied
 * to all channels and simplify implementing a global volume control.
 * 
 * @param[in]    vol            Master volume (range [0..1])
 */
void mixer_set_vol(float vol);

/**
 * @brief Set channel volume (as left/right).
 * 
 * Configure channel volume for the specified channel, specifying two values:
 * one for the left output and one for the right output.
 * 
 * The volume is an attenuation (no amplification is performed).
 * Valid volume range in [0..1], where 0 is silence and 1 is original
 * channel sample volume (no attenuation performed).
 * 
 * Notice that it's perfectly valid to set left/right volumes even if the
 * channel itself will play a mono waveforms, as it allows to balance a mono
 * sample between the two final output channels.
 * 
 * @param[in]   ch              Channel index
 * @param[in]   lvol            Left volume (range [0..1])
 * @param[in]   rvol            Right volume (range [0..1])
 */
void mixer_ch_set_vol(int ch, float lvol, float rvol);

/**
 * @brief Set channel volume (as volume and panning).
 * 
 * Configure the left and right channel volumes for the specified channel,
 * Using a central volume value and a panning value to specify left/right
 * balance.
 * 
 * Valid volume range in [0..1], where 0 is silence and 1 is maximum
 * volume (no attenuation).
 * 
 * Valid panning range is [0..1] where 0 is 100% left, and 1 is 100% right.
 * 
 * Notice that panning 0.5 balance the sound but causes an attenuation of 50%.
 * 
 * @param[in]   ch              Channel index
 * @param[in]   vol             Central volume (range [0..1])
 * @param[in]   pan             Panning (range [0..1], center is 0.5)
 */
void mixer_ch_set_vol_pan(int ch, float vol, float pan);

/**
 * @brief Set channel volume with Dolby Pro Logic II encoding.
 * 
 * Configure the volumes of the specified channel according to the Dolby Pro
 * Logic II matrix encoding. This allows to encode samples with a virtual surround
 * system, that can be decoded with a Dolby 5.1 compatible equipment.
 *
 * The function accepts the volumes configured for the 5 channels: front left,
 * front right, center, surround left, surround right. These values can be
 * calculated from a 3D scene
 * 
 * @param[in]   ch              Channel index
 * @param[in]   fl              Front left volume (range [0..1])
 * @param[in]   fr              Front right volume (range [0..1])
 * @param[in]   c               Central volume (range [0..1])
 * @param[in]   sl              Surround left volume (range [0..1])
 * @param[in]   sr              Surround right volume (range [0..1])
 */
void mixer_ch_set_vol_dolby(int ch, float fl, float fr,
	float c, float sl, float sr);

/**
 * @brief Start playing the specified waveform on the specified channel.
 * 
 * This function immediately begins playing the waveform, interrupting any
 * other waveform that might have been reproduced on this channel.
 * 
 * Waveform settings are applied to the mixer channel; for instance, the
 * frequency of the channel is modified to adapt to the frequency requested
 * for correct playback of the waveform.
 * 
 * If the waveform is marked as stereo (channels == 2), the mixer will need
 * two channels to play it back. "ch" will be used for the left samples,
 * while "ch+1" will be used for the right samples. After this, it is
 * forbidden to call mixer functions on "ch+1" until the stereo
 * waveform is stopped.
 * 
 * If the same waveform (same pointer) was already being played or was the
 * last one that was played on this channel, the channel sample buffer
 * is retained, so that any cached samples might be reused.
 * 
 * @param[in]   ch              Channel index
 * @param[in]   wave            Waveform to playback
 */
void mixer_ch_play(int ch, waveform_t *wave);

/**
 * @brief Change the frequency for the specified channel.
 * 
 * By default, the frequency is the one required by the waveform associated
 * to the channel, but this function allows to override.
 * 
 * This function must be called after #mixer_ch_play, as otherwise the
 * frequency is reset to the default of the waveform.
 * 
 * @param[in]   ch              Channel index
 * @param[in]   frequency       Playback frequency (in Hz / samples per second)
 */
void mixer_ch_set_freq(int ch, float frequency);

/** 
 * @brief Change the current playback position within a waveform. 
 * 
 * This function can be useful to seek to a specific point of the waveform.
 * The position must be specified in number of samples (not bytes). Fractional
 * values account for accurate resampling position.
 *
 * This function must be called after #mixer_ch_play, as otherwise the
 * position is reset to the beginning of the waveform.
 * 
 * @param[in]   ch              Channel index
 * @param[in]   pos             Playback position (in number of samples)
 */
void mixer_ch_set_pos(int ch, float pos);

/** 
 * Read the current playback position of the waveform in the channel.
 * 
 * The position is returned as number of samples. Fractional values account
 * for accurate resampling position. 
 * 
 * @param[in]   ch              Channel index
 * @return                      Playback position (in number of samples)
 */
float mixer_ch_get_pos(int ch);

/** @brief Stop playing samples on the specified channel. */
void mixer_ch_stop(int ch);

/** @brief  Return true if the channel is currently playing samples. */
bool mixer_ch_playing(int ch);

/**
 * @brief Configure the limits of a channel with respect to sample bit size, and
 *        frequency.
 *
 * This is an advanced function that should be used with caution, only in
 * situations in which it is paramount to control the memory usage of the mixer.
 *
 * By default, each channel in the mixer is capable of doing 16-bit playback
 * with a frequency up to the mixer output sample rate (eg: 44100hz). This means
 * that the mixer will allocate sample buffers required for this kind of
 * capability.
 *
 * If it is known that certain channels will use only 8-bit waveforms and/or
 * a lower frequency, it is possible to call this function to inform the mixer
 * of these limits. This will cause the mixer to reallocate the samplebuffers
 * lowering its memory usage (note: multiple calls to this function for different
 * channels will of course be batched to cause only one reallocation).
 *
 * Note also that this function can be used to increase the maximum frequency
 * over the mixer sample rate, in case this is required. This works correctly
 * but since it causes downsampling, it is generally a waste of memory bandwidth
 * and processing power.
 *
 * "max_buf_sz" can be used to limit the maximum buffer size that will be
 * allocated for this channel (in bytes). This is a hard cap, applied on top
 * of the optimal buffer size that will be calculated by "max_bits" and
 * "max_frequency", and can be used in situations where there are very strong
 * memory constraints that must be respected. Use 0 if you don't want to impose
 * a limit.
 * 
 * @param[in]   ch              Channel index
 * @param[in]   max_bits        Maximum number of bits per sample (or 0 to reset
 *                              this to default, which is currently 16).
 * @param[in]   max_frequency   Maximum playback frequency for this channel
 *                              in Hz / samples per seconds (or 0 to reset
 *                              this to default, which is the output sample
 *                              rate as specified in #audio_init).
 * @param[in]   max_buf_sz      Maximum buffer size in bytes (or 0 to reset
 *                              this default, which is calculated using the
 *                              other limits, the playback output rate, and
 *                              the number of audio buffers specified in
 *                              #audio_init).
 */
void mixer_ch_set_limits(int ch, int max_bits, float max_frequency, int max_buf_sz);

/**
 * @brief Throttle the mixer by specifying the maximum number of samples
 *        it can generate.
 * 
 * This is an advanced function that should only be called to achieve perfect
 * sync between a possibly slowing-down video and audio.
 * 
 * Normally, once the mixer is initiated and assuming mixer_poll is called
 * frequently enough, the audio will playback uninterrupted, irrespective of
 * any slow down in the main loop. This is the expected behavior for background
 * music for instance, but it does not work for video players or cut-scenes in
 * which the music must be perfectly synchronized with the video. If the video
 * happens to slowdown, the music will desynchronize.
 * 
 * mixer_throttle sets a budget of samples that the mixer is allowed to
 * generate. Every time the function is called, the specified number of samples
 * is added to the budget. Every time the mixer playbacks the channel, the
 * budget is decreased. If the budget reaches zero, the mixer will automatically
 * pause playback until the budget is increased again, possibly creating
 * audio cracks.
 * 
 * To achieve perfect sync, call #mixer_throttle every time a video frame
 * was generated, and pass the maximum number of samples that the mixer is
 * allowed to produce. Typically, you will want to pass the audio samplerate
 * divided by the video framerate, which corresponds to the number of
 * audio samples per video frame.
 * 
 * @param[in]   num_samples     Number of new samples that the mixer is allowed
 *                              to produce for this channel. This will be added
 *                              to whatever allowance was left.
 *                              
 * @see #mixer_unthrottle
 */
void mixer_throttle(float num_samples);

/**
 * @brief Unthrottle the mixer
 * 
 * Switch back the mixer to the default unthrottled status, after some calls to
 * #mixer_throttle.
 * 
 * After calling #mixer_unthrottle, the mixer will no longer be limited and
 * will produce all the samples requested via #mixer_poll.
 * 
 * @see #mixer_throttle
 */
void mixer_unthrottle(void);

/**
 * @brief Run the mixer to produce output samples.
 * 
 * This function will fetch the required samples from all the channels and
 * mix them together according to each channel's settings. The output will
 * be written into the specified buffer (out). nsamples is the number of
 * samples that should be produced.
 * 
 * A common pattern would be to call #audio_write_begin to obtain an audio
 * buffer's pointer, and pass it to mixer_poll.
 *
 * mixer_poll performs mixing using RSP. If RSP is busy, mixer_poll will
 * spin-wait until the RSP is free, to perform audio processing.
 *
 * Since the N64 AI can only be fed with an even number of samples, mixer_poll
 * does not accept odd numbers.
 * 
 * This function will respect throttling, if configured via #mixer_throttle.
 * In this case, it may produce less samples than requested, depending on
 * the current allowance. The rest of the output buffer will be zeroed.
 * 
 * @param[in]   out             Output buffer were samples will be written.
 * @param[in]   nsamples        Number of stereo samples to generate.
 */
void mixer_poll(int16_t *out, int nsamples);

/**
 * @brief Request the mixer to try and write audio samples to be played,
 * if possible.
 * 
 * This function is a user helper for asking the mixer and audio subsystems
 * to play audio during a game frame. You should call this function many times
 * during one frame (eg. during the render step or after processing each game
 * object) as many times as necessary. Not polling the audio subsystem often
 * enough will result in audio stutter. 
 */
void mixer_try_play();

/**
 * @brief Callback invoked by mixer_poll at a specified time
 * 
 * A MixerEvent is a callback that is invoked during mixer_poll at a specified
 * moment in time (that is, after a specified number of samples have been
 * processed). It is useful to implement sequencers that need to update their
 * status after a certain number of samples, or effects like volume envelopes
 * that must update the channel volumes at regular intervals.
 *
 * "ctx" is an opaque pointer that provides a context to the callback.
 *
 * If the callback has finished its task, it can return 0 to deregister itself
 * from the mixer. Otherwise, it can return a positive number of samples to
 * wait before calling it again.
 * 
 * @param[in]   ctx             Opaque pointer to provide context (specified
 *                              in #mixer_add_event)
 * @return                      0 if the event can be deregistered, otherwise
 *                              a positive number of samples to wait before
 *                              calling again.
 */
typedef int (*MixerEvent)(void* ctx);

/**
 * @brief Register a time-based event into the mixer.
 * 
 * Register a new event into the mixer. "delay" is the number of samples to
 * wait before calling the event callback. "cb" is the event callback. "ctx"
 * is an opaque pointer that will be passed to the callback when invoked.
 * 
 * @param[in]   delay           Number of samples to wait before invoking
 *                              the event.
 * @param[in]   cb              Event callback to invoke
 * @param[in]   ctx             Context opaque pointer to pass to the callback
 */
void mixer_add_event(int64_t delay, MixerEvent cb, void *ctx);

/**
 * @brief Deregister a time-based event from the mixer.
 * 
 * Deregister an event from the mixer. "cb" is the event callback, and "ctx"
 * is the opaque context pointer. Notice that an event can also deregister
 * itself by returning 0 when called.
 * 
 * @param[in]    cb             Callback that was registered via #mixer_add_event
 * @param[in]    ctx            Opaque pointer that was registered with the callback.
 */
void mixer_remove_event(MixerEvent cb, void *ctx);


/*********************************************************************
 *
 * WAVEFORMS
 *
 *********************************************************************/

/**
 * @brief Waveform callback function invoked by the mixer to read/generate samples.
 * 
 * WaveformRead is a callback function that will be invoked by the mixer
 * whenever a new unavailable portion of the waveform is requested.
 *
 * *wpos* indicates the absolute position in the waveform from which
 * the function must start reading, and *wlen* indicates the minimum number of
 * samples to read.
 *
 * The read function should push into the provided sample buffer
 * at least *wlen* samples, using #samplebuffer_append. Producing more samples
 * than requested is perfectly fine, they will be stored in the sample buffer
 * and remain available for later use. For instance, a compressed waveform
 * (eg: VADPCM) might decompress samples in blocks of fixed size, and thus
 * push full blocks into the sample buffer, to avoid decoding a block twice.
 *
 * On the contrary, producing less samples should only be done
 * if the read function has absolutely no way to produce more. This should
 * happen only when the stream is finished, and only when the waveform length
 * was unknown to the mixer (otherwise, the mixer won't ask more samples than
 * available in the first place).
 *
 * The argument *seeking* is a flag that indicates whether the read being
 * requested requires seeking or not. If *seeking* is false (most of the times),
 * it means that the current read is requesting samples which come immediately
 * after the ones that were last read; in other words, the implementation might
 * decide to ignore the *wpos* argument and simply continue decoding the audio
 * from the place were it last stopped. If *seeking* is true, instead, it means
 * that there was a jump in position that should be taken into account.
 *
 * Normally, a seeking is required in the following situations:
 *
 *   * At start (#mixer_ch_play): the first read issued by the mixer will
 *     be at position 0 with `seeking == true` (unless the waveform was
 *     already partially cached in the sample buffer's channel).
 *   * At user request (#mixer_ch_set_pos).
 *   * At loop: if a loop is specified in the waveform (`loop_len != 0`), the
 *     mixer will seek when required to execute the loop.
 *
 * Notice that producing more samples than requested in *wlen* might break
 * the 8-byte buffer alignment guarantee that #samplebuffer_append tries to
 * provide. For instance, if the read function is called requesting 24 samples,
 * but it produces 25 samples instead, the alignment in the buffer will be lost,
 * and next call to #samplebuffer_append will return an unaligned pointer.
 *
 * @param[in]  ctx     Opaque pointer that is provided as context to the function,
 *                     and is specified in the waveform.
 * @param[in]  sbuf    Samplebuffer into which read samples should be stored.
 * @param[in]  wpos    Absolute position in the waveform to read from (in samples).
 * @param[in]  wlen    Minimum number of samples to read (in samples).
 * @param[in]  seeking True if this call requires seeking in the waveform, false
 *                     if this read is consecutive to the last one.
 */
typedef void (*WaveformRead)(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking);

/**
 * @brief A waveform that can be played back through the mixer.
 * 
 * waveform_t represents a waveform that can be played back by the mixer.
 * A waveform_t does not hold the actual samples because most real-world use
 * cases do not keep all samples in memory, but rather load them and/or
 * decompress them in real-time while the playback is happening.
 * So waveform_t instead should be thought of as the generator of a
 * waveform.
 *
 * To create a waveform, use one of waveform implementations such as wav64. 
 * Waveform implementations are in charge of generating the samples by actually
 * implementing an audio format like VADPCM or MPEG-2.
 *
 * Waveforms can produce samples as 8-bit or 16-bit. Samples must always be
 * signed. Stereo waveforms (interleaved samples) are supported: when used
 * with #mixer_ch_play, they will use automatically two channels (the specified
 * one and the following).
 */
typedef struct waveform_s {
	/** @brief Name of the waveform (for debugging purposes) */
	const char *name;

	/** 
     * @brief Width of a sample of this waveform, in bits.
     * 
     * Supported values are 8 or 16. Notice that samples must always be signed.
     */
	uint8_t bits;

	/** 
     * @brief Number of interleaved audio channels in this waveforms.
     * 
     * Supported values are 1 and 2 (mono and stereo waveforms). Notice that
     * a stereo waveform will use two consecutive mixer channels to be played back.
     */
	uint8_t channels;

	/** @brief Desired playback frequency (in samples per second, aka Hz). */
	float frequency;

	/**
     * @brief Length of the waveform, in number of samples.
     * 
     * If the length is not known, this value should be set to #WAVEFORM_UNKNOWN_LEN.
     */
	int len;

	/**
     * @brief Length of the loop of the waveform (from the end).
     * 
     * This value describes how many samples of the tail of the waveform needs
     * to be played in a loop. For instance, if len==1200 and loop_len=500, the
     * waveform will be played once, and then the last 700 samples will be
     * repeated in loop.
     */
	int loop_len;

	/** 
     * @brief Read function of the waveform.
     * 
     * This is the callback that will be invoked by the mixer to generate the
     * samples. See #WaveformRead for more information.
     */
	WaveformRead read;

	/** @brief Opaque pointer provided as context to the read function. */
	void *ctx;
} waveform_t;

/** @brief Maximum number of samples in a waveform */
#define WAVEFORM_MAX_LEN         0x1FFFFFFF

/** 
 * @brief Specify that the waveform length is unknown.
 * 
 * This value can be used in the "len" field of #waveform_t to specify
 * that the waveform length is unknown. The mixer will be unable to perform
 * looping so the #WaveformRead function will have to handle looping by itself
 * and/or generate silence once the playback is finished.
 */
#define WAVEFORM_UNKNOWN_LEN     WAVEFORM_MAX_LEN

#ifdef __cplusplus
}
#endif

#endif /* __LIBDRAGON_MIXER_H */
