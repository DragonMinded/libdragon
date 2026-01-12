/**
 * @file h264.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief H264 video player using RSP acceleration
 */

#include "h264.h"
#include "h264_decoder.h"
#include "rsph264_internal.h"
#include "asset_internal.h"
#include "video_internal.h"
#include "n64sys.h"
#include "profile.h"
#include "yuv.h"
#include "surface.h"
#include "rspq.h"
#include "debug.h"
#include "utils.h"
#include <unistd.h>
#include <string.h>

// The size of the internal I/O buffer in the H264 decoder. This must be
// at least MAX_SLICE_SIZE, but hopefully much more than that (so that
// less shuffling is required for each slice).
#define H264_BUF_SIZE         (64*1024)

#define H264BSD_EOF           (-1)

typedef struct h264_s {
    video_t video;                      ///< Video handle
    uint8_t buf[H264_BUF_SIZE];         ///< Internal buffered data
    uint8_t *pic;                       ///< Pointer to the decoded picture data
    int idx;                            ///< Current index in the buffer
    int buf_len;                        ///< Number of valid bytes in the buffer
    storage_t s;                        ///< H264 decoder main structure
    int fd;                             ///< File descriptor of the opened H264 file
    bool in_frame_decoding;             ///< True if we have partially decoded a frame 
    uint32_t max_slice_size;            ///< Maximum size of a slice seen so far
    int max_buffered_pics;              ///< Runtime-configured buffered pictures
} h264_t;

static void release_current_picture(h264_t *player) {
    if (player->pic) {
        /* The DPB keeps output pictures locked until the client releases them,
           so that background decoding cannot overwrite the current picture. */
        h264bsdDpbReleasePicture(player->s.dpb, player->pic);
        player->pic = NULL;
    }
}

static int decode_next_slice(h264_t *player) {
    int left = player->buf_len - player->idx;

    // If there's not enough data for a full slice, we need to compact the
    // buffer and do a I/O from ROM.
    if (left <= player->max_slice_size) {
        memmove(player->buf, player->buf+player->idx, left);
        int n = read(player->fd, player->buf+left, H264_BUF_SIZE-left);
        if (n < 0) n = 0;
        player->buf_len = left + n;
        player->idx = 0;

        if (player->buf_len == 0)
            return H264BSD_EOF;
    }

    // Do the actual decoding
    unsigned int np;
    PROFILE_START(PS_H264);
    int status = h264bsdDecode(&player->s, player->buf+player->idx, player->buf_len-player->idx, 0, &np);
    PROFILE_STOP(PS_H264);

    player->idx += np;
    player->max_slice_size = MAX(player->max_slice_size, np*1.3f);

    return status;
}

static bool fetch_picture(h264_t *player) {
    unsigned int picId=0, isIdrPic=0, numErrs=0;
    player->pic = h264bsdNextOutputPicture(&player->s, &picId, &isIdrPic, &numErrs);
    assert(numErrs == 0);
    return player->pic != NULL;
}


static float get_framerate(h264_t *player) {
    if (player->s.activeSps && 
        player->s.activeSps->vuiParametersPresentFlag && 
        player->s.activeSps->vuiParameters && 
        player->s.activeSps->vuiParameters->timingInfoPresentFlag) {
        
        uint32_t timeScale = player->s.activeSps->vuiParameters->timeScale;
        uint32_t numUnitsInTick = player->s.activeSps->vuiParameters->numUnitsInTick;
        
        if (numUnitsInTick > 0) {
            return (float)timeScale / (2.0f * numUnitsInTick);
        }
    }
    // Default to 20 fps if not specified. This is totally arbitrary, but
    // better than no playback...
    return 20.0f;
}

static float get_aspect_ratio(h264_t *player) {
    // Prefer VUI SAR if present; otherwise assume square pixels.
    u32 sarW = 1, sarH = 1;
    h264bsdSampleAspectRatio(&player->s, &sarW, &sarH);
    if (sarW == 0 || sarH == 0) {
        sarW = 1;
        sarH = 1;
    }
    return ((float)player->video.info.width * (float)sarW) / 
           ((float)player->video.info.height * (float)sarH);
}

static yuv_colorspace_t get_colorspace(h264_t *player) {

    // H.264 VUI:
    // - matrix_coefficients (if present; otherwise "unspecified" = 2)
    // - video_full_range_flag (0 => limited/TV, 1 => full/PC)
    const u32 full = h264bsdVideoRange(&player->s);
    const u32 m = h264bsdMatrixCoefficients(&player->s);

    // matrix_coefficients values (H.264): 1=BT.709, 5=BT.470BG, 6=SMPTE170M, 2=unspecified.
    const bool is_bt709 = (m == 1);
    // Treat unspecified and BT.601 family as BT.601.
    if (is_bt709) {
        return full ? YUV_BT709_FULL : YUV_BT709_TV;
    } else {
        return full ? YUV_BT601_FULL : YUV_BT601_TV;
    }
}

static void h264_rewind(video_t *v) {
    h264_t *player = (h264_t*)v;

    release_current_picture(player);
    lseek(player->fd, 0, SEEK_SET);
    player->idx = 0;
    player->buf_len = 0;
    player->in_frame_decoding = false;
    
    // Reset decoder
    h264bsdInit(&player->s, 0);

    rsph264_begin_frame();
    while (1) {
        int status = decode_next_slice(player);
        switch (status) {
            case H264BSD_RDY:
                continue;
            case H264BSD_HDRS_RDY:
                player->in_frame_decoding = true;
                return;
            default:
                assertf(0, "h264 rewind error: %d", status);
                return;
        }
    }
}

static video_t* h264_open(const char *fn) {
    h264_t *player = malloc(sizeof(h264_t));
    assertf(player, "Out of memory");
    sys_hw_memset(player, 0, sizeof(h264_t));
    player->fd = -1;
    player->max_buffered_pics = 4; /* TODO: make configurable by the caller */

    rsph264_init();
    h264bsdInitStorage(&player->s);
    h264bsdSetNumBufferedPics(&player->s, (u32)player->max_buffered_pics);
    
    player->fd = must_open(fn);
    h264_rewind(&player->video);

    // Fetch video information
    player->video.info.width =  h264bsdPicWidth(&player->s) * 16;
    player->video.info.height = h264bsdPicHeight(&player->s) * 16;
    player->video.info.framerate = get_framerate(player);
    player->video.info.aspect_ratio = get_aspect_ratio(player);
    player->video.info.colorspace = get_colorspace(player);

    return &player->video;
}


static void h264_close(video_t *v) {
    h264_t *player = (h264_t*)v;
    if (player->fd >= 0) {
        close(player->fd);
        player->fd = -1;
    }
    free(player);
}

static const char* h264_status_str(int status) {
    switch (status) {
    case H264BSD_RDY: return "H264BSD_RDY";
    case H264BSD_PIC_RDY: return "H264BSD_PIC_RDY";
    case H264BSD_HDRS_RDY: return "H264BSD_HDRS_RDY";
    case H264BSD_ERROR: return "H264BSD_ERROR";
    case H264BSD_PARAM_SET_ERROR: return "H264BSD_PARAM_SET_ERROR";
    case H264BSD_MEMALLOC_ERROR: return "H264BSD_MEMALLOC_ERROR";
    default: return "???";
    }
}

typedef enum {
    POLL_NOTHING = 0,
    POLL_DECODING = 1,
    POLL_READY = 2,
    POLL_EOF = -1,
} poll_status_t;

static poll_status_t poll(h264_t *player)
{
    // If the output buffer is full, we can't decode more, as there
    // wouldn't be space for further pictures. Just exit.
    if (player->max_buffered_pics > 0 &&
        h264bsdDpbNumOutputPictures(player->s.dpb) >= (u32)player->max_buffered_pics)
        return POLL_NOTHING;

    int status = decode_next_slice(player);

    switch (status) {
    case H264BSD_EOF:
        // Video is ended
        return POLL_EOF;
    case H264BSD_RDY:
        player->in_frame_decoding = true;
        return POLL_DECODING;
    case H264BSD_PIC_RDY:
        player->in_frame_decoding = false;
        return POLL_READY;
    default:
        assertf(!"h264 status error", "status == %d (%s)\n", status, h264_status_str(status));
        return POLL_NOTHING;
    }
}

static bool poll_loop(h264_t *player)
{
    while (1) {
        poll_status_t status = poll(player);
        assertf(status != POLL_NOTHING, "Decoder stalled while decoding frame");
        if (status == POLL_EOF)
            return false;
        if (status == POLL_READY)
            return true;
    }
}

static int h264_poll(video_t *v)
{
    h264_t *player = (h264_t*)v;

    /* If buffering is disabled, do not decode in background. Decoding will
       still happen on-demand via h264_next_frame()/poll_loop(). */
    if (player->max_buffered_pics == 0)
        return 0;

    switch (poll(player))
    {
        case POLL_DECODING:
        case POLL_READY:
            return 1;
        case POLL_NOTHING:
        case POLL_EOF:
        default:
            return 0;
    }
}

static bool h264_next_frame(video_t *v)
{
    h264_t *player = (h264_t*)v;

    /* Release previous frame, if any. This makes the DPB buffer slot available
       again for background decoding. */
    release_current_picture(player);

    // debugf("buffered: %d\n", h264bsdDpbNumOutputPictures(player->s.dpb));

    // Fetch one picture from the output buffer. It could be pending because
    // it's the first frame, or because previous calls to decode_loop have
    // decoded more than one picture
    if (fetch_picture(player))
        return true;

    // If no picture was decoded yet, we need to keep decoding until
    // one picture is ready.
    if (!poll_loop(player))
        return false;

    bool fetched = fetch_picture(player);
    assertf(fetched, "Failed to fetch picture after decoding");
    return true;
}

static yuv_frame_t h264_get_frame(video_t *v)
{
    h264_t *player = (h264_t*)v;

    int w = player->video.info.width, h = player->video.info.height;
    int w2 = w / 2, h2 = h / 2;

    return (yuv_frame_t){
        .y = surface_make(player->pic,                   FMT_I8, w,  h,  w),
        .u = surface_make(player->pic + w * h,           FMT_I8, w2, h2, w2),
        .v = surface_make(player->pic + w * h + w2 * h2, FMT_I8, w2, h2, w2),
    };
}

static void h264_seekfast(video_t *v, int frame_idx, uint32_t file_off)
{
    h264_t *player = (h264_t*)v;

    /* Discard the current frame (if any) */
    release_current_picture(player);

	// Finish decoding the current frame, if any. This is required if we're
	// using poll() and we are in the middle of a single frame decoding
	// (or if we've just initialized the player and we're in the middle
	// of the first frame).
	if (player->in_frame_decoding) {
		poll_loop(player);
        assert(!player->in_frame_decoding);
    }

	// Flush all pending pictures
	while (fetch_picture(player)) {
        /* We're discarding these pictures, so release them immediately. */
        release_current_picture(player);
    }

    // Seek the underlying file to the specified offset, so that we're
	// ready for next frame.
	lseek(player->fd, file_off, SEEK_SET);
    player->idx = 0;
    player->buf_len = 0;
}    

void __h264_profile_init(void) {
	profile_register(PS_H264, "H264", 0);
	profile_register(PS_H264_NAL, "NAL", 1);
	profile_register(PS_H264_MACROB, "MacroB", 1);
	profile_register(PS_H264_LAYER, "Layer", 1);
	profile_register(PS_H264_LAYER_CLEAR, "Clear", 2);
	profile_register(PS_H264_LAYER_PRED, "Predict", 2);
	profile_register(PS_H264_LAYER_RES, "Residual", 2);
	profile_register(PS_H264_LAYER_RES_ENC, "Encode", 3);
	profile_register(PS_H264_RESIDUAL_LUMA, "Residual Luma", 3);
	profile_register(PS_H264_RESIDUAL_CHROMA, "Residual Chroma", 3);
	profile_register(PS_H264_INTRAPRED_4X4, "IntraPred 4x4", 4);
	profile_register(PS_H264_INTRAPRED_16X16, "IntraPred 16x16", 4);
	profile_register(PS_H264_INTERPRED, "InterPred", 2);
	profile_register(PS_H264_INTERPRED_LUMA, "InterPred Luma", 3);
	profile_register(PS_H264_INTERPRED_CHROMA, "InterPred Chroma", 3);
}

video_codec_t h264_codec = {
    .extension = ".h264",
    .open = h264_open,
    .close = h264_close,
    .poll = h264_poll,
    .get_frame = h264_get_frame,
    .next_frame = h264_next_frame,
    .rewind = h264_rewind,
    .seekfast = h264_seekfast,
};
