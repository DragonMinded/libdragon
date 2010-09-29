#ifndef __LIBDRAGON_AUDIO_H
#define __LIBDRAGON_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the audio subsystem to a desired frequency. Do not call this
   twice without a call to audio_close as this will result in a memory leak */
void audio_init(const int frequency);

/* Write a buffer to the audio system.  To find out the proper length of the
   buffer to pass to this function, call audio_get_buffer_length().  This
   returns the number of samples needed.  Remember that samples are short ints
   and stereo interleaved. */
void audio_write(const short * const buffer);

/* Return whether or not there is room to write another sample */
volatile int audio_can_write();

/* Write silence to the audio system.  This is provided as a convenience so the
   programmer does not have to allocate a buffer, zero it and then pass it in.  */
void audio_write_silence();

/* Close and free memory reserved by the audio system */
void audio_close();

/* Returns the frequency in Hz that the audio system is configured to work at */
int audio_get_frequency();

/* Returns the length in samples of a given buffer in the audio system.  Buffers
   passed in through audio_write() should use this to determine the desired
   length.  This returns the length in number of samples.  Remember that N64
   audio is stereo interleaved. */
int audio_get_buffer_length();

#ifdef __cplusplus
}
#endif

#endif
