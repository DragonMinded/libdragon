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
#include "asset.h"
#include <sys/stat.h>

static video_codec_t *registered_codecs = NULL;

typedef struct video_seektable_s {
    char magic[3];              ///< "VSK" magic
    uint8_t version;            ///< Version of the seek table format
    int num_entries;            ///< Number of entries in the seek table
    uint32_t *file_offsets;     ///< Array of file offsets for keyframes
    int *frame_idx;             ///< Array of frame indices for keyframes
    uint32_t mean_offset;       ///< Mean of the offsets deltas
    uint32_t mean_frame;        ///< Mean of the frame deltas
} video_seektable_t;

/** @brief Decode a pointer relative to object start */
#define PTR_DECODE(font, ptr)    ((void*)(((uint8_t*)(font)) + (uint32_t)(ptr)))

static video_seektable_t *video_seektable_load(const char *fn)
{
    video_seektable_t *tbl = asset_load(fn, NULL);
    assertf(memcmp(tbl->magic, "VSK", 3) == 0, "Invalid seek table file: %s", fn);
    assertf(tbl->version == 2, "Unsupported seek table version %d in file: %s", tbl->version, fn);

    tbl->file_offsets = PTR_DECODE(tbl, tbl->file_offsets);
    tbl->frame_idx    = PTR_DECODE(tbl, tbl->frame_idx);

    // Decode residuals to absolute values. The file stores residuals from the mean
    // of the deltas to improve compressibility, but we need absolute values at runtime.
    for (int i=1; i< tbl->num_entries; i++) {
        tbl->file_offsets[i] += tbl->file_offsets[i-1] + tbl->mean_offset;
        tbl->frame_idx[i]    += tbl->frame_idx[i-1]    + tbl->mean_frame;
    }

    return tbl;
}

/** 
 * @brief Lookup the seektable for a specified frame.
 * 
 * This function performs a binary search on the seek table to find the
 * file offset corresponding to the specified frame index. If the exact frame
 * index is not found, it returns the file offset of the closest preceding
 * keyframe.
 * 
 * @param tbl          Pointer to the seek table
 * @param frame_idx    Frame index to seek to. On return, it contains the actual
 *                     frame index found in the seek table (which might be
 *                     different from the requested one).
 * @return uint32_t    File offset of the keyframe
 */
static uint32_t video_seektable_lookup(video_seektable_t *tbl, int *frame_idx)
{
    int left = 0;
    int right = tbl->num_entries - 1;
    int result_idx = -1;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (tbl->frame_idx[mid] == *frame_idx) {
            result_idx = mid;
            break;
        } else if (tbl->frame_idx[mid] < *frame_idx) {
            result_idx = mid; // Keep track of the closest preceding keyframe
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    if (result_idx >= 0) {
        *frame_idx = tbl->frame_idx[result_idx];
        return tbl->file_offsets[result_idx];
    } else {
        // No valid keyframe found; return start of the file
        *frame_idx = 0;
        return 0;
    }
}

static void video_seektable_free(video_seektable_t *tbl)
{
    free(tbl);
}

void video_register_codec(video_codec_t *codec)
{
    if (codec->next_codec != NULL) {
        debugf("video_register_codec: codec for %s already registered\n", codec->extension);
        return;
    }
    codec->next_codec = registered_codecs;
    registered_codecs = codec;
}

video_t* video_open(const char *fn, const video_parms_t *parms)
{
    video_codec_t *codec = registered_codecs;

    const char *ext = strrchr(fn, '.');
    assertf(ext, "File %s has no extension", fn);

    while (codec) {
        if (strcmp(codec->extension, ext) == 0) {
            video_t *v = codec->open(fn, parms);
            if (v) {
                v->codec = codec;

                // If the codec support fast seeking, check if there is a seek
                // table for this video
                if (v->codec->seekfast) {                
                    int fnlen = strlen(fn), extlen = strlen(ext);
                    char *seekfn = alloca(fnlen - extlen + strlen(".seek") + 1);
                    strcpy(seekfn, fn);
                    strcpy(seekfn + (fnlen - extlen), ".seek");

                    struct stat st;
                    if (stat(seekfn, &st) == 0) {
                        v->seektable = video_seektable_load(seekfn);
                    }
                }
            }
            return v;
        }
        codec = codec->next_codec;
    }
    
    assertf(false, "No registered codec found for video file: %s", fn);
    return NULL;
}

void video_close(video_t *v)
{
    if (v->seektable) {
        video_seektable_free(v->seektable);
        v->seektable = NULL;
    }
    v->codec->close(v);
}

video_info_t video_get_info(video_t *v)
{
    return v->info;
}

int video_poll(video_t *v)
{
    if (!v->codec->poll) return 0;
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

int video_seek(video_t *v, int frame_idx)
{
    if (v->seektable) {
        int keyframe_idx = frame_idx;
        uint32_t keyframe_off = video_seektable_lookup(v->seektable, &keyframe_idx);
        v->codec->seekfast(v, keyframe_idx, keyframe_off);
        return keyframe_idx;
    }

    if (v->codec->seek) {
        return v->codec->seek(v, frame_idx);
    }
    
    return -1;
}

bool video_is_seekable(video_t *v, int frame_idx)
{
    if (!v->seektable) return false;

    int found = frame_idx;
    video_seektable_lookup(v->seektable, &found);
    return found == frame_idx;
}

