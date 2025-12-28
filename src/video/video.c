/**
 * @file video.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Video player subsystem
 * 
 * 
 */

#include "video.h"
#include "video_internal.h"
#include "debug.h"

static video_codec_t *registered_codecs = NULL;

void video_register_codec(video_codec_t *codec)
{
    codec->next_codec = registered_codecs;
    registered_codecs = codec;
}

video_t* video_open(const char *fn)
{
    video_codec_t *codec = registered_codecs;

    const char *ext = strrchr(fn, '.');
    assertf(ext, "File %s has no extension", fn);

    while (codec) {
        if (strcmp(codec->extension, ext) == 0) {
            video_t *v = codec->open(fn);
            if (v) v->codec = codec;
            return v;
        }
        codec = codec->next_codec;
    }
    
    assertf(false, "No registered codec found for video file: %s", fn);
}

void video_close(video_t *v)
{
    v->codec->close(v);
}

video_info_t video_get_info(video_t *v)
{
    return v->info;
}

int video_poll(video_t *v)
{
    return v->codec->poll(v);
}

bool video_next_frame(video_t *v)
{
    return v->codec->next_frame(v);
}

yuv_frame_t video_get_frame(video_t *v)
{
    return v->codec->get_frame(v);
}

void video_rewind(video_t *v)
{
    v->codec->rewind(v);
}

int video_seek_frame(video_t *v, int frame_idx)
{
    return v->codec->seek(v, frame_idx);
}

float video_seek_time(video_t *v, float time_sec)
{
    int target_frame = (int)(time_sec * v->info.framerate);
    int actual_frame = video_seek_frame(v, target_frame);
    return (float)actual_frame / v->info.framerate;
}
