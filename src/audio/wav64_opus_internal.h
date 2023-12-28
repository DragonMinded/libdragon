#ifndef LIBDRAGON_AUDIO_WAV64_OPUS_INTERNAL_H
#define LIBDRAGON_AUDIO_WAV64_OPUS_INTERNAL_H

#include <stdint.h>
#include "wav64.h"

void wav64_opus_init(wav64_t *wav, int fh);
void wav64_opus_close(wav64_t *wav);

#endif
