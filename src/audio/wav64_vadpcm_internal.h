#ifndef LIBDRAGON_AUDIO_VADPCM_INTERNAL_H
#define LIBDRAGON_AUDIO_VADPCM_INTERNAL_H

void wav64_vadpcm_init(wav64_t *wav);
void wav64_vadpcm_close(wav64_t *wav);
int wav64_vadpcm_get_bitrate(wav64_t *wav);

#endif
