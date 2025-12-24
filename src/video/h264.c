/**
 * @file h264.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief H264 video player using RSP acceleration
 */

#include "h264.h"
#include "h264_decoder.h"
#include "rsph264_internal.h"
#include "asset_internal.h"
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
    uint8_t buf[H264_BUF_SIZE];         ///< Internal buffered data
    uint8_t *pic;                       ///< Pointer to the decoded picture data
    int idx;                            ///< Current index in the buffer
    int buf_len;                        ///< Number of valid bytes in the buffer
    storage_t s;                        ///< H264 decoder main structure
    int fd;                             ///< File descriptor of the opened H264 file
    int width;                          ///< Width of the video       
    int height;                         ///< Height of the video
    bool in_frame_decoding;             ///< True if we have partially decoded a frame 
    uint32_t max_slice_size;            ///< Maximum size of a slice seen so far
} h264_t;

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
    int status = h264bsdDecode(&player->s, player->buf+player->idx, player->buf_len-player->idx, 0, &np);

    player->idx += np;
    player->max_slice_size = MAX(player->max_slice_size, np*1.3f);

    return status;
}

static void fetch_picture(h264_t *player) {
    unsigned int picId=0, isIdrPic=0, numErrs=0;
    player->pic = h264bsdNextOutputPicture(&player->s, &picId, &isIdrPic, &numErrs);
    assert(numErrs == 0);
}


h264_t* h264_open(const char *fn) {
    h264_t *player = malloc(sizeof(h264_t));
    sys_hw_memset(player, 0, sizeof(h264_t));
    player->fd = -1;

    rsph264_init();

    h264bsdInitStorage(&player->s);
    
    player->fd = must_open(fn);
    
    h264_rewind(player);
    
    return player;
}

int h264_get_width(h264_t *player) {
    return player->width;
}

int h264_get_height(h264_t *player) {
    return player->height;
}

float h264_get_framerate(h264_t *player) {
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
    return 0.0f;
}

float h264_get_aspect_ratio(h264_t *player) {
    // Prefer VUI SAR if present; otherwise assume square pixels.
    u32 sarW = 1, sarH = 1;
    h264bsdSampleAspectRatio(&player->s, &sarW, &sarH);
    if (sarW == 0 || sarH == 0) {
        sarW = 1;
        sarH = 1;
    }

    return ((float)player->width * (float)sarW) / ((float)player->height * (float)sarH);
}

yuv_colorspace_t h264_get_colorspace(h264_t *player) {
    if (!player) return YUV_BT601_TV;

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

void h264_rewind(h264_t *player) {
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
                player->width = h264bsdPicWidth(&player->s) * 16;
                player->height = h264bsdPicHeight(&player->s) * 16;
                player->in_frame_decoding = true;
                rsph264_end_frame();
                return;
            default:
                assertf(0, "h264 rewind error: %d", status);
                return;
        }
    }
}

void h264_close(h264_t *player) {
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

static int decode_loop(h264_t *player, uint64_t deadline) {
    // Check whether the RSP is idle. If it's not, wait for it and account
    // that time to the YUV blitter.
    // Normally, the RSP is already idle by the time we get here, which
    // is exactly what we want: zero wait time.
    PROFILE_START(PS_YUV, 0);
    rspq_wait();
    PROFILE_STOP(PS_YUV, 0);

    rsph264_begin_frame();
    while (!deadline || (get_ticks() < deadline)) {
        // If the output buffer is full, we can't decode more, as there
        // wouldn't be space for further pictures. Just exit.
        if (h264bsdDpbNumOutputPictures(player->s.dpb) >= MAX_NUM_BUFFERED_PICS)
            break;

        int status = decode_next_slice(player);
    
        switch (status) {
        case H264BSD_EOF:
            rsph264_end_frame();
            return 0;
        case H264BSD_RDY:
            player->in_frame_decoding = true;
            break;
        case H264BSD_PIC_RDY:
            player->in_frame_decoding = false;
            if (!deadline)
                goto end;
            break;
        default:
            assertf(!"h264 status error", "status == %d (%s)\n", status, h264_status_str(status));
        }
    }
end:
    rsph264_end_frame();
    return 1;
}

void h264_poll(h264_t *player, uint64_t deadline) {
    decode_loop(player, deadline);
}

bool h264_next_frame(h264_t *player) {
    // Fetch one picture from the output buffer. It could be pending because
    // it's the first frame, or because previous calls to decode_loop have
    // decoded more than one picture
    fetch_picture(player);

    // If no picture was decoded yet, we need to keep decoding until
    // one picture is ready.
    if (player->pic == NULL) {
        int ret = decode_loop(player, 0);  // decode until one picture is decoded
        fetch_picture(player);
        if (player->pic == NULL) {
            if (ret == 0) return false;
            assert(player->pic != NULL);
        }
    }

    return true;
}

yuv_frame_t h264_get_frame(h264_t *v)
{
    int w = v->width, h = v->height;
    int w2 = w / 2, h2 = h / 2;

    return (yuv_frame_t){
        .y = surface_make(v->pic,                   FMT_I8, w,  h,  w),
        .u = surface_make(v->pic + w * h,           FMT_I8, w2, h2, w2),
        .v = surface_make(v->pic + w * h + w2 * h2, FMT_I8, w2, h2, w2),
    };
}
