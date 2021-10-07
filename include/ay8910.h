#ifndef __LIBDRAGON_AY8910_H
#define __LIBDRAGON_AY8910_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Decimation factor for AY8910. 
 * 
 * AY8910 is usually clocked at a very high frequency (>100K), and thus it
 * requires downsampling to be played back. This emulator offers a very basic
 * downsampling filter via decimation (taking average of Nth consecutive samples)
 * that is usually a good compromise between quality and speed, for realtime
 * playback. It will not sound as good as a real downsampling filter though.
 * 
 * It is suggested to configure this number to the smallest value that brings
 * the AY8910 output frequency within the playback sample rate.
 */
#define AY8910_DECIMATE           3

/** @brief Generate stereo output.
 * 
 * If 1, AY8910 will generate a stereo output with fixed pans for
 * each of the three internal channels, similar to what Arkos Tracker 2 does
 * in stereo mode.
 */
#define AY8910_OUTPUT_STEREO      1

/** @brief Define the global attenuation applied to volumes (range 0.0 - 1.0). 
 * 
 * The AY8910 often clips so it's important to lower a bit the volume to avoid
 * sound artifacts.
 */
#define AY8910_VOLUME_ATTENUATE   0.8

/** @brief Generate silence as 0.
 * 
 * Normally, AY8910 generates output samples in which the silence is
 * represented by -32768 (minimum volume). This is a little inconvenient
 * if the caller wants to skip generation when the AY8910 is muted for
 * performance reasons, because audio mixers normally assume that muted
 * channels are made by samples with value 0, otherwise disabling the
 * AY8910 would affect the volume of all other channels.
 *
 * By setting this macro to 1, the dynamic range will be halved in the range
 * 0-32767, so the silence will be as expected, but the audio will
 * be somehow "duller".
 */
#define AY8910_CENTER_SILENCE     1

typedef struct {
	uint16_t tone_period;
	uint8_t tone_vol;
	uint8_t tone_en;
	uint8_t noise_en;

	uint16_t count;
	uint8_t out;

	uint8_t prev_vol;
	uint8_t prev_count;
} AYChannel;

typedef struct {
	uint16_t period;
	uint8_t shape;

	int16_t step;
	uint8_t attack;
	uint8_t alternate, hold, holding;

	uint16_t count;
	uint8_t vol;
} AYEnvelope;

typedef struct {
	uint8_t period;

	uint8_t count;
	uint32_t out;
} AYNoise;

typedef struct {
	uint8_t (*PortRead)(int idx);
	void (*PortWrite)(int idx, uint8_t val);
	uint8_t addr;
	uint8_t regs[16];
	AYChannel ch[3];
	AYNoise ns;
	AYEnvelope env;
} AY8910;

/** @brief Reset the AY8910 emulator. */
void ay8910_reset(AY8910 *ay);

/** @brief Configure the I/O port callbacks. */
void ay8910_set_ports(AY8910 *ay, uint8_t (*PortRead)(int), void (*PortWrite)(int, uint8_t));

/** @brief Write to the AY8910 address line. */
void ay8910_write_addr(AY8910 *ay, uint8_t addr);

/** @brief Write to the AY8910 data line. */
void ay8910_write_data(AY8910 *ay, uint8_t val);

/** @brief Read from the AY8910 data line. */
uint8_t ay8910_read_data(AY8910 *ay);

/** @brief Return true if ay8910 is currently muted (all channels disabled) */
bool ay8910_is_mute(AY8910 *ay);

/** @brief Generate audio for the specified number of samples.
 * 
 * "nsamples" is the number of samples after decimation (so the exact number of
 * samples written in the output buffer).
 */
int ay8910_gen(AY8910 *ay, int16_t *out, int nsamples);

#ifdef __cplusplus
}
#endif

#endif
