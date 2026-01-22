#define LIBDRAGON_PROFILE 0
#include <libdragon.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/video/fastcache.h"
#include "../src/video/rsph264_internal.h"
#include "../src/video/h264_decoder/h264bsd_macroblock_layer.h"
#include "../src/video/h264_decoder/h264bsd_util.h"
#include "../src/video/h264_decoder/h264bsd_stream.h"

// Include files for the reference implementation.
#include "../src/video/h264_decoder/omxdl/armVC.h"
#if 1
#include "../src/video/h264_decoder/omxdl/armCOMM.c"
#include "../src/video/h264_decoder/omxdl/armVCCOMM_Average.c"
#include "../src/video/h264_decoder/omxdl/armVCM4P10_Interpolate_Luma.c"
#include "../src/video/h264_decoder/omxdl/armVCM4P10_Interpolate_Chroma.c"
#include "../src/video/h264_decoder/omxdl/armVCM4P10_InterpolateHalfHor_Luma.c"
#include "../src/video/h264_decoder/omxdl/armVCM4P10_InterpolateHalfVer_Luma.c"
#include "../src/video/h264_decoder/omxdl/armVCM4P10_InterpolateHalfDiag_Luma.c"
#include "../src/video/h264_decoder/omxdl/omxVCM4P10_PredictIntra_4x4.c"
#include "../src/video/h264_decoder/omxdl/omxVCM4P10_PredictIntra_16x16.c"
#include "../src/video/h264_decoder/omxdl/omxVCM4P10_PredictIntraChroma_8x8.c"
#include "../src/video/h264_decoder/omxdl/omxVCM4P10_TransformDequantChromaDCFromPair.c"
#include "../src/video/h264_decoder/omxdl/omxVCM4P10_DequantTransformResidualFromPairAndAdd.c"
#endif

#define SRC_SIZE 256
#define SRC_PITCH (256+64*2)
#define DST_SIZE 512

#define INTERPOLATE_CHROMA 0
#define INTERPOLATE_LUMA   1

#define INTRAPRED_LUMA_4X4   0
#define INTRAPRED_LUMA_16X16 1
#define INTRAPRED_CHROMA_8X8 2
#define INTRAPRED_PROCESS_LUMA4 3
#define INTRAPRED_PROCESS_LUMA16 4

#define OMX_LUMA_4x4       0
#define OMX_CHROMADC_2x2   1
#define OMX_LUMADC_4x4     2
#define PROCESS_LUMA_16x16 3
#define PROCESS_CHROMA_8x8x2 4

typedef struct {
    uint8_t *pSrc1, *pSrc2;  // source buffers 
    uint8_t *pDst1, *pDst2;  // destination buffers
} BufferTest;

typedef struct {
    BufferTest buf;
    int func;
    int x2, y2;
    int qp;
    uint8_t *src;
    uint8_t ac[27] __attribute__((aligned(8)));
    int16_t *dc;
} DequantTest;
 
typedef struct {
    BufferTest buf;          // buffers
    int func;                // interpolation function to test
    int x1, y1, x2, y2;      // position in source/destination
    int w, h;                // macroblock size
    int dx, dy;              // fractional offset
} InterpolationTest;

typedef struct {
    BufferTest buf;
    int func;                // intraprediction function to test
    int x1, y1, x2, y2;      // position in source/destination
    int mode[16];            // intraprediction mode (for each partition)
    int avail[16];           // neighbour availability (for each partition)
    uint8_t *coeffs;         // residual coefficients
    uint8_t ac[27];          // residual presence
    int qp;                  // quantization factor (for residuals)
} IntraPredictionTest;

static uint32_t rand_state = 1;
static void my_srand(uint32_t val) {
    if (val == 0) val = 1;
    rand_state = val;
}
static uint32_t my_rand() {
    uint32_t x = rand_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return rand_state = x;
}

// Measure the time that takes to execute a statement.
#define TIME_STATEMENT(stmt) ({ \
	uint32_t start, stop; \
	start = TICKS_READ(); \
	stmt; \
	stop = TICKS_READ(); \
	TICKS_DISTANCE(start, stop); \
})

bool interpolation_test(InterpolationTest* test, int verbose) {
    uint8_t *src1 = test->buf.pSrc1+test->y1*SRC_PITCH+test->x1;
    uint8_t *src2 = test->buf.pSrc2+test->y1*SRC_PITCH+test->x1;

    uint8_t *dst1 = test->buf.pDst1+test->y2*DST_SIZE+test->x2;
    uint8_t *dst2 = test->buf.pDst2+test->y2*DST_SIZE+test->x2;

    int frame_width = SRC_SIZE;
    int frame_height = SRC_SIZE;

    uint32_t rsp_time = 0, ref_time = 0;
    switch (test->func) {
    case INTERPOLATE_LUMA:
        rsph264_queue_debug_random_status();
        rsph264_sync();

        rsp_time = TIME_STATEMENT({
            // Calculate the motion vector as the signed difference between
            // x2/y2 and x1/y1. This is what happens in reality: the position
            // is the one in the destination buffer, and the motion vector is
            // the relative distance in the source buffer.
            int16_t mvx = (((int16_t)test->x1 - (int16_t)test->x2) << 2) | test->dx;
            int16_t mvy = (((int16_t)test->y1 - (int16_t)test->y2) << 2) | test->dy;

            rsph264_queue_interpolate_luma_overfill(RSPH264_CACHE_SKIP_SOURCE,
                test->buf.pSrc1, SRC_PITCH, dst1, DST_SIZE,
                (frame_width << 16) | frame_height,  // frame size
                (test->w << 16) | test->h,           // block size
                ((uint32_t)mvx << 16) | ((uint32_t)(mvy & 0xFFFF)), // motion vector
                ((uint32_t)test->x2 << 16) | ((uint32_t)test->y2)  // position
            );

            rsph264_sync();
        });

        ref_time = TIME_STATEMENT({
            armVCM4P10_Interpolate_Luma(
                src2, SRC_PITCH, dst2, DST_SIZE,
                test->w, test->h, test->dx, test->dy);
        });
        break;

    case INTERPOLATE_CHROMA:
        rsph264_queue_debug_random_status();
        rsph264_sync();

        rsp_time = TIME_STATEMENT({
            // Calculate the motion vector as the signed difference between
            // x2/y2 and x1/y1. This is what happens in reality: the position
            // is the one in the destination buffer, and the motion vector is
            // the relative distance in the source buffer.
            int16_t mvx = (((int16_t)test->x1 - (int16_t)test->x2) << 3) | test->dx;
            int16_t mvy = (((int16_t)test->y1 - (int16_t)test->y2) << 3) | test->dy;

            rsph264_queue_interpolate_chroma_overfill(RSPH264_CACHE_SKIP_SOURCE,
                test->buf.pSrc1, SRC_PITCH, dst1, DST_SIZE,
                (frame_width << 16) | frame_height,  // frame size
                (test->w << 16) | test->h,           // block size
                ((uint32_t)mvx << 16) | ((uint32_t)(mvy & 0xFFFF)), // motion vector
                ((uint32_t)test->x2 << 16) | ((uint32_t)test->y2)  // position
            );

            rsph264_sync();
        });

        ref_time = TIME_STATEMENT({
            armVCM4P10_Interpolate_Chroma(
                src2, SRC_PITCH, dst2, DST_SIZE,
                test->w, test->h, test->dx, test->dy);
        });
        break;

    default:
        assert(0);
    }

    if (verbose >= 2)
        debugf("sz:%d,%d d:%d,%d rsp:%ld ref:%ld\n", test->w, test->h, test->dx, test->dy, rsp_time, ref_time);
    for (int j = -4; j < 32; j++) {
        for (int i = -4; i < 32; i++) {
            uint8_t *cdst = (uint8_t*)dst1;
            if (cdst[j*DST_SIZE+i] != dst2[j*DST_SIZE+i]) {
                if (verbose >= 1) {   
                    printf("FAILED\n");
                    printf("FAILED: sz:%d,%d d:%d,%d\n", test->w, test->h, test->dx, test->dy);
                    printf("FAILED: difference at (%d,%d)\n", i, j);
                    printf("FAILED: src:(%d,%d) dst:(%d,%d)\n", test->x1, test->y1, test->x2, test->y2);
                    printf("FAILED: RSP=%02x    REF=%02x\n", cdst[j*DST_SIZE+i], dst2[j*DST_SIZE+i]);
                    printf("SRC:        RSP:         REF:\n");
                    printf("%02x %02x %02x    %02x %02x %02x     %02x %02x %02x\n", 
                        src2[(j-1)*SRC_PITCH+i-1], src2[(j-1)*SRC_PITCH+i], src2[(j-1)*SRC_PITCH+i+1],
                        cdst[(j-1)*DST_SIZE+i-1], cdst[(j-1)*DST_SIZE+i], cdst[(j-1)*DST_SIZE+i+1],
                        dst2[(j-1)*DST_SIZE+i-1], dst2[(j-1)*DST_SIZE+i], dst2[(j-1)*DST_SIZE+i+1]);
                    printf("%02x %02x %02x    %02x %02x %02x     %02x %02x %02x\n", 
                        src2[j*SRC_PITCH+i-1], src2[j*SRC_PITCH+i], src2[j*SRC_PITCH+i+1],
                        cdst[j*DST_SIZE+i-1], cdst[j*DST_SIZE+i], cdst[j*DST_SIZE+i+1],
                        dst2[j*DST_SIZE+i-1], dst2[j*DST_SIZE+i], dst2[j*DST_SIZE+i+1]);
                    printf("%02x %02x %02x    %02x %02x %02x     %02x %02x %02x\n", 
                        src2[(j+1)*SRC_PITCH+i-1], src2[(j+1)*SRC_PITCH+i], src2[(j+1)*SRC_PITCH+i+1],
                        cdst[(j+1)*DST_SIZE+i-1], cdst[(j+1)*DST_SIZE+i], cdst[(j+1)*DST_SIZE+i+1],
                        dst2[(j+1)*DST_SIZE+i-1], dst2[(j+1)*DST_SIZE+i], dst2[(j+1)*DST_SIZE+i+1]);
                }
                return false;
            }
        }
    }
    return true;
}

static uint32_t chroma_partitions[7][2] = {
    { 8, 8 },
    { 8, 4 },
    { 4, 8 },
    { 4, 4 },
    { 2, 4 },
    { 4, 2 },
    { 2, 2 },
};

void exhaustive_interpolation_test(InterpolationTest *test, int repetitions, int verbose) {
    int max_dxy = (test->func == INTERPOLATE_LUMA) ? 4 : 8;
    for (int p=0;p<7;p++) {
        uint32_t w = chroma_partitions[p][0];
        uint32_t h = chroma_partitions[p][1];

        if (test->func == INTERPOLATE_LUMA) {            
            w*=2; h*=2;
        }

        for (uint32_t dy=0;dy<max_dxy;dy++) {
            for (uint32_t dx=0;dx<max_dxy;dx++) {
                for (int nt=0;nt<repetitions;nt++) {
                    int v = verbose;
                    if (verbose == 2 && nt != repetitions-1)
                        v = 1;

                    test->w = w;
                    test->h = h;

                    // Source coordinates can span also the overfill area (outside the screen).
                    // Technically, they could be any 12-bit signed value, but the reference
                    // implementation does not support overfilling and we fake it by pre-overfilling
                    // the source buffer (which has a limited size).
                    test->x1 = (my_rand() % (SRC_SIZE+64)) - 32;
                    test->y1 = (my_rand() % (SRC_SIZE+64)) - 32;
                    test->x2 = (my_rand() % (DST_SIZE-64)) + 32;
                    test->y2 = (my_rand() % (DST_SIZE-64)) + 32;
                    test->x2 &= ~(w-1);  // destination buffer is always aligned to macroblock width
                    test->dx = dx;
                    test->dy = dy;
                    bool ok = interpolation_test(test, v);
                    if (!ok) {
                        printf("FAILED: nt=%d\n", nt);
                        while(1) {}
                    }
                }
            }
        }
    }
}

int gen_coeff_delta(uint8_t *coeffs, int cmask, int max_coeff) {
    int cidx = 0;
    int ncoeffs = (my_rand() % (max_coeff-1)) + 1;
    for (int i=0;i<ncoeffs;i++) {
        coeffs[cidx++] = my_rand() % max_coeff;
        if (i == ncoeffs-1) {
            coeffs[cidx-1] |= 0x20;
        }
        int16_t c = (my_rand() & cmask) - (cmask+1)/2;

        if ((int8_t)c == c) {
            coeffs[cidx++] = (uint8_t)c;
        } else {            
            coeffs[cidx-1] |= 0x10;
            coeffs[cidx++] = c & 0xFF;
            coeffs[cidx++] = c >> 16;
        }
    }
    return cidx;
}

bool intrapred_test(IntraPredictionTest *test, int verbose) {
    uint8_t *src1 = test->buf.pSrc1+test->y1*SRC_PITCH+test->x1;
    uint8_t *src2 = test->buf.pSrc2+test->y1*SRC_PITCH+test->x1;

    uint8_t *dst1 = test->buf.pDst1+test->y2*DST_SIZE+test->x2;
    uint8_t *dst2 = test->buf.pDst2+test->y2*DST_SIZE+test->x2;

    const uint8_t *csrc1 = test->coeffs;
    const uint8_t *csrc2 = test->coeffs;

    uint32_t rsp_time = 0, ref_time = 0;

    switch (test->func) {
    case INTRAPRED_LUMA_4X4:
        rsph264_queue_debug_random_status();
        rsph264_sync();

        rsp_time = TIME_STATEMENT({
            rsph264_queue_intrapred_luma_4x4(0,
                src1-1, src1-SRC_PITCH, src1-SRC_PITCH-1,
                dst1, SRC_PITCH, DST_SIZE,
                test->mode[0], test->avail[0]);
            rsph264_sync();
        });

        ref_time = TIME_STATEMENT({
            int res = omxVCM4P10_PredictIntra_4x4(
                src2-1, src2-SRC_PITCH, src2-SRC_PITCH-1,
                dst2, SRC_PITCH, DST_SIZE,
                test->mode[0], test->avail[0]);
            assert(res == 0);
        });
        break;

    case INTRAPRED_LUMA_16X16:
        rsph264_queue_debug_random_status();
        rsph264_sync();

        rsp_time = TIME_STATEMENT({
            rsph264_queue_intrapred_luma_16x16(0,
                src1-1, src1-SRC_PITCH, src1-SRC_PITCH-1,
                dst1, SRC_PITCH, DST_SIZE,
                test->mode[0], test->avail[0]);
            rsph264_sync();
        });

        ref_time = TIME_STATEMENT({
            int res = omxVCM4P10_PredictIntra_16x16(
                src2-1, src2-SRC_PITCH, src2-SRC_PITCH-1,
                dst2, SRC_PITCH, DST_SIZE,
                test->mode[0], test->avail[0]);
            assert(res == 0);
        });
        break;

    case INTRAPRED_CHROMA_8X8:
        rsph264_queue_debug_random_status();
        rsph264_sync();

        rsp_time = TIME_STATEMENT({
            rsph264_queue_intrapred_chroma_8x8(0,
                src1-1, src1-SRC_PITCH, src1-SRC_PITCH-1,
                dst1, SRC_PITCH, DST_SIZE,
                test->mode[0], test->avail[0]);
            rsph264_sync();
        });

        ref_time = TIME_STATEMENT({
            int res = omxVCM4P10_PredictIntraChroma_8x8(
                src2-1, src2-SRC_PITCH, src2-SRC_PITCH-1,
                dst2, SRC_PITCH, DST_SIZE,
                test->mode[0], test->avail[0]);
            assert(res == 0);
        });
        break;

    case INTRAPRED_PROCESS_LUMA4: 
    {
        rsph264_queue_debug_random_status();
        rsph264_sync();

        uint8_t modeAvail[16];
        for (int i=0;i<16;i++) {
            // We only care about 4 availability bits: UPPER / LEFT / UPPER_LEFT / UPPER_RIGHT
            uint8_t avail = test->avail[i] & 3;
            if (test->avail[i] & (1<<5))
                avail |= (1<<2);
            if (test->avail[i] & (1<<6))
                avail |= (1<<3);
            modeAvail[i] = avail | (test->mode[i]<<4);
        }

        rsp_time = TIME_STATEMENT({
            rsph264_queue_set_packed_delta_buffer(0,
                csrc1);
            rsph264_queue_process_luma_intra4_residual(0,
                src1, dst1, SRC_PITCH, DST_SIZE,
                modeAvail, test->qp, h264bsdTotalCoeffMask(test->ac));
            rsph264_sync();
        });

        ref_time = TIME_STATEMENT({
            int res = HIGHFUNC_PredictIntraTransform_4x4(
                src2, dst2, SRC_PITCH, DST_SIZE,
                &csrc2, test->ac,
                modeAvail, test->qp);
            assert(res == 0);
        });
        }
        break;

    case INTRAPRED_PROCESS_LUMA16:
    {
        rsph264_queue_debug_random_status();
        rsph264_sync();

        rsp_time = TIME_STATEMENT({
            rsph264_queue_set_packed_delta_buffer(0,
                csrc1);
            rsph264_queue_process_luma_intra16_residual(0,
                src1, dst1, SRC_PITCH, DST_SIZE,
                test->mode[0], test->avail[0], test->qp, h264bsdTotalCoeffMask(test->ac));
            rsph264_sync();
        });

        ref_time = TIME_STATEMENT({
            int res = HIGHFUNC_ProcessLumaIntra16x16Residual(
                src2, dst2, SRC_PITCH, DST_SIZE,
                &csrc2, test->ac,
                test->mode[0], test->avail[0], test->qp);
            assert(res == 0);
        });

        break;       
    } 

    default:
        assert(0);
    }

    if (verbose >= 2)
        debugf("m:%d rsp:%ld ref:%ld\n", test->mode[0], rsp_time, ref_time);
    for (int j = -4; j < 32; j++) {
        for (int i = -4; i < 32; i++) {
            uint8_t *cdst = (uint8_t*)dst1;
            if (cdst[j*DST_SIZE+i] != dst2[j*DST_SIZE+i]) {
                if (verbose >= 1) {   
                    printf("FAILED\n");
                    printf("FAILED: mode:%d avail:%x\n", test->mode[0], test->avail[0]);
                    printf("FAILED: difference at (%d,%d)\n", i, j);
                    printf("FAILED: src:(%d,%d) dst:(%d,%d)\n", test->x1, test->y1, test->x2, test->y2);
                    printf("FAILED: RSP=%02x    REF=%02x\n", cdst[j*DST_SIZE+i], dst2[j*DST_SIZE+i]);
                    printf("SRC:        RSP:         REF:\n");
                    printf("%02x %02x %02x    %02x %02x %02x     %02x %02x %02x\n", 
                        src2[(j-1)*SRC_PITCH+i-1], src2[(j-1)*SRC_PITCH+i], src2[(j-1)*SRC_PITCH+i+1],
                        cdst[(j-1)*DST_SIZE+i-1], cdst[(j-1)*DST_SIZE+i], cdst[(j-1)*DST_SIZE+i+1],
                        dst2[(j-1)*DST_SIZE+i-1], dst2[(j-1)*DST_SIZE+i], dst2[(j-1)*DST_SIZE+i+1]);
                    printf("%02x %02x %02x    %02x %02x %02x     %02x %02x %02x\n", 
                        src2[j*SRC_PITCH+i-1], src2[j*SRC_PITCH+i], src2[j*SRC_PITCH+i+1],
                        cdst[j*DST_SIZE+i-1], cdst[j*DST_SIZE+i], cdst[j*DST_SIZE+i+1],
                        dst2[j*DST_SIZE+i-1], dst2[j*DST_SIZE+i], dst2[j*DST_SIZE+i+1]);
                    printf("%02x %02x %02x    %02x %02x %02x     %02x %02x %02x\n", 
                        src2[(j+1)*SRC_PITCH+i-1], src2[(j+1)*SRC_PITCH+i], src2[(j+1)*SRC_PITCH+i+1],
                        cdst[(j+1)*DST_SIZE+i-1], cdst[(j+1)*DST_SIZE+i], cdst[(j+1)*DST_SIZE+i+1],
                        dst2[(j+1)*DST_SIZE+i-1], dst2[(j+1)*DST_SIZE+i], dst2[(j+1)*DST_SIZE+i+1]);
                }
                return false;
            }
        }
    }
    return true;

}


void exhaustive_intrapred_test(IntraPredictionTest *test, int repetitions, int verbose) {
    static uint8_t coeffs[2048];
    int count = 0;
    int nummodes = 0;
    int numblocks = 0;

    switch (test->func) {
    // The first 3 functions are single-block intraprediction.
    case INTRAPRED_LUMA_4X4:   nummodes = 9; numblocks = 1; break;
    case INTRAPRED_LUMA_16X16: nummodes = 4; numblocks = 1; break;
    case INTRAPRED_CHROMA_8X8: nummodes = 4; numblocks = 1; break;
    // This function covers the whole macroblock (with 16 4x4 blocks)
    case INTRAPRED_PROCESS_LUMA4: nummodes = 9; numblocks = 16; break;
    case INTRAPRED_PROCESS_LUMA16: nummodes = 4; numblocks = 16; break;
    default: assert(0);
    }

    for (int m=0;m<nummodes;m++) {
        for (int nt=0;nt<repetitions;nt++) {
            my_srand(1024 + count++);
            int v = verbose;
            if (verbose == 2 && nt != repetitions-1)
                v = 1;

            test->x1 = (my_rand() % (SRC_SIZE-64)) + 32;
            test->y1 = (my_rand() % (SRC_SIZE-64)) + 32;
            test->x2 = (my_rand() % (DST_SIZE-64)) + 32;
            test->y2 = (my_rand() % (DST_SIZE-64)) + 32;

            // Prepare data for residual coefficients
            test->qp = my_rand()%48;  // FIXME: should be 52
            int src_offset = my_rand() % 8;
            int cidx = src_offset;
            test->coeffs = coeffs + src_offset;

            // The bigger the shift factor, the smaller the coefficients,
            // otherwise we risk overflow which is not correctly handled
            // by the reference implementation (assuming it's permitted
            // by the standard in the first place... I have not found
            // information on this).
            int cmask = 0x1FF >> (test->qp/6);

            memset(test->ac, 0, sizeof(test->ac));
            if (test->func == INTRAPRED_PROCESS_LUMA16) {
                // For INTRAPRED_PROCESS_LUMA16, we first prepare also input data 
                // for LumaDC coefficients.
                test->ac[24] = my_rand()%2;
                if (test->ac[24] != 0) {
                    cidx += gen_coeff_delta(coeffs+cidx, cmask, 16);
                }
            }

            for (int b=0;b<numblocks;b++) {
                test->mode[b] = m;
                if (b != 0 && my_rand() % 5 == 0) {
                    // Every once in a while, change mode for some block.
                    // This is actually quite common, so we want to cover
                    // the case in which not all blocks have the same mode.
                    // At the same time, the test is structured to test mode
                    // by mode, so don't do that very often.
                    test->mode[b] = my_rand()%nummodes;
                }
                test->avail[b] = OMX_VC_UPPER|OMX_VC_LEFT|OMX_VC_UPPER_LEFT;

                // 4x4 blocks within the macroblock are processed in this order:
                //   xx xx xx xx xx xx xx xx xx
                //   0  1  8  9
                //   2  3  10 11
                //   4  5  12 13
                //   6  7  14 15
                //
                // UPPER_RIGHT cannot be set:
                //   * On blocks 3 and 7 because when they are processed,
                //     their upper-right block (8 and 12) have not been
                //     processed yet.
                //   * On blocks 11, 13 and 15 because they are upper right
                //     data would be in the macroblock on the right which
                //     is not yet processed.
                if (b != 3 && b != 7 && b != 11 && b != 13 && b != 15) {
                    if (my_rand()%2 == 0)
                        test->avail[b] |= OMX_VC_UPPER_RIGHT;
                }

                if (test->func == INTRAPRED_LUMA_4X4) {
                    test->x1 &= ~3; test->x2 &= ~3;
                    if (test->mode[b] == 2) { // OMX_VC_4X4_DC
                        if (my_rand()%2 == 0)
                            test->avail[b] &= ~OMX_VC_UPPER;
                        if (my_rand()%2 == 0)
                            test->avail[b] &= ~OMX_VC_LEFT;
                    }                
                } else if (test->func == INTRAPRED_LUMA_16X16) {
                    test->x1 &= ~15; test->x2 &= ~15;
                    if (test->mode[b] == 2) { // OMX_VC_16X16_DC
                        if (my_rand()%2 == 0)
                            test->avail[b] &= ~OMX_VC_UPPER;
                        if (my_rand()%2 == 0)
                            test->avail[b] &= ~OMX_VC_LEFT;
                    }                
                } else if (test->func == INTRAPRED_CHROMA_8X8) {
                    test->x1 &= ~7; test->x2 &= ~7;
                    if (test->mode[b] == 0) { // OMX_VC_CHROMA_DC
                        if (my_rand()%2 == 0)
                            test->avail[b] &= ~OMX_VC_UPPER;
                        if (my_rand()%2 == 0)
                            test->avail[b] &= ~OMX_VC_LEFT;
                    }                
                } else if (test->func == INTRAPRED_PROCESS_LUMA4 || test->func == INTRAPRED_PROCESS_LUMA16) {
                    test->x1 &= ~15; test->x2 &= ~15;
                    if (test->mode[b] == 2) { // OMX_VC_4X4_DC
                        if (my_rand()%2 == 0)
                            test->avail[b] &= ~OMX_VC_UPPER;
                        if (my_rand()%2 == 0)
                            test->avail[b] &= ~OMX_VC_LEFT;
                    }

                    // If there's LumaDC (only for INTRAPRED_PROCESS_LUMA16),
                    // then we have max 15 coefficients per block. The 16th
                    // is the (0,0) coeff we get from the LumaDC matrix.
                    int maxCoeff = test->ac[24] ? 15 : 16;

                    // For INTRAPRED_PROCESS_LUMA4/16 we need to also
                    // create coefficients.
                    test->ac[b] = my_rand()%2;
                    if (test->ac[b] != 0) {
                        cidx += gen_coeff_delta(coeffs+cidx, cmask, maxCoeff);
                    }
                }
            }

            bool ok = intrapred_test(test, v);
            if (!ok) {
                printf("FAILED: test:%d\n", nt);
                while(1) {}
            }
        }
    }    
}

void overfill_interpolation_test(InterpolationTest *test, int verbose) {
    int max_dxy = (test->func == INTERPOLATE_LUMA) ? 4 : 8;
    for (int p=0;p<7;p++) {
        for (int dx=0;dx<max_dxy;dx++) {
            for (int dy=0;dy<max_dxy;dy++) {        
                uint32_t w = chroma_partitions[p][0];
                uint32_t h = chroma_partitions[p][1];

                if (test->func == INTERPOLATE_LUMA) {            
                    w*=2; h*=2;
                }

                test->w = w;
                test->h = h;
                test->dx = dx;
                test->dy = dy;
                test->x2 = 16;
                test->y2 = 16;

                // Left border
                for (int x = -24; x < 4; x++) {
                    test->x1 = x;
                    test->y1 = (x&3)-1;
                    bool ok = interpolation_test(test, verbose);
                    if (!ok) while(1) {}
                }

                // Right border
                for (int x = SRC_SIZE-w-4; x < SRC_SIZE+8; x++) {
                    test->x1 = x;
                    test->y1 = SRC_SIZE-1-(x&3);
                    bool ok = interpolation_test(test, verbose);
                    if (!ok) while(1) {}
                }

                // Top border
                for (int y = -24; y < 4; y++) {
                    test->x1 = SRC_SIZE-1-(y&3);
                    test->y1 = y;
                    bool ok = interpolation_test(test, verbose);
                    if (!ok) while(1) {}
                }

                // Bottom border
                for (int y = SRC_SIZE-w-4; y < SRC_SIZE+8; y++) {
                    test->x1 = (y&3)-1;
                    test->y1 = y;
                    bool ok = interpolation_test(test, verbose);
                    if (!ok) while(1) {}
                }
            }
        }
    }
}

bool dequant_test(DequantTest *test, int verbose) {
    #define MAX_DEQUANT_CHECK_SIZE 64
    int check_size = MAX_DEQUANT_CHECK_SIZE;
    uint8_t *dst1 = test->buf.pDst1 + test->y2*DST_SIZE + test->x2;
    uint8_t *dst2 = test->buf.pDst2 + test->y2*DST_SIZE + test->x2;

    const uint8_t *csrc1 = test->src;
    const uint8_t *csrc2 = test->src;

    uint32_t rsp_time = 0, ref_time = 0;

    switch (test->func) {
    case OMX_LUMA_4x4:        
        rsph264_queue_debug_random_status();
        rsph264_sync();

        rsp_time = TIME_STATEMENT({
            rsph264_queue_set_packed_delta_buffer(0,
                csrc1);
            rsph264_queue_dequant_transform_residual(0,
                dst1, DST_SIZE,
                test->dc, test->qp, test->ac[0]);
            rsph264_sync();
        });

        ref_time = TIME_STATEMENT({
            OMXResult err = omxVCM4P10_DequantTransformResidualFromPairAndAdd(
                &csrc2,
                dst2, test->dc, dst2, DST_SIZE, DST_SIZE,
                test->qp, test->ac[0]);
            assert(err == OMX_Sts_NoErr);
        });
        break;

    case PROCESS_LUMA_16x16:
        rsph264_queue_debug_random_status();
        rsph264_sync();

        rsp_time = TIME_STATEMENT({
            rsph264_queue_set_packed_delta_buffer(0,
                csrc1);
            rsph264_queue_process_luma_inter_residual(0,
                dst1, DST_SIZE,
                0, test->qp, h264bsdTotalCoeffMask(test->ac));
            rsph264_sync();
        });

        ref_time = TIME_STATEMENT({
            OMXResult err = HIGHFUNC_ProcessLumaInterResidual(
                &csrc2, 
                dst2, dst2, DST_SIZE, DST_SIZE,
                test->qp, test->ac);
            assert(err == OMX_Sts_NoErr);
        });
        break;

    case PROCESS_CHROMA_8x8x2:
        rsph264_queue_debug_random_status();
        rsph264_sync();

        rsp_time = TIME_STATEMENT({
            rsph264_queue_set_packed_delta_buffer(0,
                csrc1);
            rsph264_queue_process_chroma_residual(0,
                dst1, dst1+16, DST_SIZE,
                test->qp, h264bsdTotalCoeffMask(test->ac));
            rsph264_sync();
        });

        ref_time = TIME_STATEMENT({
            OMXResult err = HIGHFUNC_ProcessChromaResidual(
                &csrc2, 
                dst2, dst2+16,
                DST_SIZE, DST_SIZE,
                test->qp, test->ac);
            assert(err == OMX_Sts_NoErr);
        });
        break;


    default:
        assert(0);
    }

    if (verbose >= 2) {
        debugf("rsp=%ld ref=%ld\n", rsp_time, ref_time);
    }
    for (int j = -check_size/2; j < check_size/2; j++) {
        for (int i = -check_size/2; i < check_size/2; i++) {
            uint8_t *cdst = (uint8_t*)dst1;
            if (cdst[j*DST_SIZE+i] != dst2[j*DST_SIZE+i]) {
                if (verbose >= 1) {   
                    printf("FAILED\n");
                    //printf("FAILED: sz:%d,%d d:%d,%d\n", test->w, test->h, test->dx, test->dy);
                    printf("FAILED: difference at (%d,%d)\n", i, j);
                    printf("FAILED: dst:(%d,%d) qp:(%d,%d)\n", test->x2, test->y2, test->qp/6, test->qp%6);
                    printf("FAILED: RSP=%02x    REF=%02x\n", cdst[j*DST_SIZE+i], dst2[j*DST_SIZE+i]);
                    printf("RSP:         REF:\n");
                    printf("%02x %02x %02x     %02x %02x %02x\n", 
                        cdst[(j-1)*DST_SIZE+i-1], cdst[(j-1)*DST_SIZE+i], cdst[(j-1)*DST_SIZE+i+1],
                        dst2[(j-1)*DST_SIZE+i-1], dst2[(j-1)*DST_SIZE+i], dst2[(j-1)*DST_SIZE+i+1]);
                    printf("%02x %02x %02x     %02x %02x %02x\n", 
                        cdst[j*DST_SIZE+i-1], cdst[j*DST_SIZE+i], cdst[j*DST_SIZE+i+1],
                        dst2[j*DST_SIZE+i-1], dst2[j*DST_SIZE+i], dst2[j*DST_SIZE+i+1]);
                    printf("%02x %02x %02x     %02x %02x %02x\n", 
                        cdst[(j+1)*DST_SIZE+i-1], cdst[(j+1)*DST_SIZE+i], cdst[(j+1)*DST_SIZE+i+1],
                        dst2[(j+1)*DST_SIZE+i-1], dst2[(j+1)*DST_SIZE+i], dst2[(j+1)*DST_SIZE+i+1]);
                }
                return false;
            }
        }
    }
    return true;
}

bool dequant_dc_test(DequantTest *test, int verbose) {
    static uint16_t dc1[16] __attribute__((aligned(8)));
    static uint16_t dc2[16] __attribute__((aligned(8)));
    const uint8_t *csrc2 = test->src;

    for (int i=0;i<16;i++)
        dc1[i] = dc2[i] = (uint16_t)my_rand();

    rsph264_queue_debug_random_status();
    rsph264_sync();

    uint32_t rsp_time = 0, ref_time = 0;
    switch (test->func) {
    case OMX_CHROMADC_2x2:
        ref_time = TIME_STATEMENT({
            OMXResult err = omxVCM4P10_TransformDequantChromaDCFromPair(
                &csrc2,
                (int16_t*)dc2, test->qp);
            assert(err == OMX_Sts_NoErr);
        });
        rsp_time = TIME_STATEMENT({
            rsph264_queue_set_packed_delta_buffer(0,
                test->src);
            rsph264_queue_transform_dequant_chromadc(0,
                (int16_t*)dc1, test->qp);
            rsph264_sync();
        });

        break;

    case OMX_LUMADC_4x4:
        rsp_time = TIME_STATEMENT({
            rsph264_queue_set_packed_delta_buffer(0,
                test->src);
            rsph264_queue_transform_dequant_lumadc(0,
                (int16_t*)dc1, test->qp);
            rsph264_sync();
        });

        ref_time = TIME_STATEMENT({
            OMXResult err = omxVCM4P10_TransformDequantLumaDCFromPair(
                &csrc2,
                (int16_t*)dc2, test->qp);
            assert(err == OMX_Sts_NoErr);
        });
        break;

    default:
        assert(0);
    }

    if (verbose >= 2) {
        debugf("rsp=%ld ref=%ld\n", rsp_time, ref_time);
    }

    for (int i=0;i<16;i++) {
        if (dc1[i] != dc2[i]) {
            if (verbose >= 1) {   
                printf("FAILED\n");
                printf("FAILED: difference at %d\n", i);
                printf("RSP: %04x %04x %04x %04x\n",
                    dc1[0],dc1[1],dc1[2],dc1[3]);
                printf("RSP: %04x %04x %04x %04x\n",
                    dc1[4],dc1[5],dc1[6],dc1[7]);
                printf("REF: %04x %04x %04x %04x\n",
                    dc2[0],dc2[1],dc2[2],dc2[3]);
                printf("REF: %04x %04x %04x %04x\n",
                    dc2[4],dc2[5],dc2[6],dc2[7]);
            }
            return false;
        }
    }
    return true;
}

bool exhaustive_dequant_test(BufferTest *buf, int func, int numtests, int verbose) {
    static uint8_t coeffs[1024];
    DequantTest test;
    test.buf = *buf;

    for (int i = 0; i < numtests; i++) {
        my_srand(i+1024);
        test.x2 = (my_rand() % (DST_SIZE-MAX_DEQUANT_CHECK_SIZE)) + MAX_DEQUANT_CHECK_SIZE/2;
        test.y2 = (my_rand() % (DST_SIZE-MAX_DEQUANT_CHECK_SIZE)) + MAX_DEQUANT_CHECK_SIZE/2;
        if (func == PROCESS_LUMA_16x16 || func == PROCESS_CHROMA_8x8x2)
            test.x2 &= ~7;
        else
            test.x2 &= ~3;
        test.func = func;

        int src_offset = my_rand() % 8;
        int cidx = src_offset;

        // The bigger the shift factor, the smaller the coefficients,
        // otherwise we risk overflow which is not correctly handled
        // by the reference implementation (assuming it's permitted
        // by the standard in the first place... I have not found
        // information on this).
        test.qp = my_rand()%48;  // FIXME: should be 52
        int cmask = 0x1FF >> (test.qp/6);

        // Test with or without DC
        int16_t dc0 = (my_rand() % 512) - 256;
        test.dc = &dc0;
        if ((my_rand()%2) == 0) test.dc = 0;

        if (test.dc && (my_rand()%32) == 0) {
            for (int i=0;i<27;i++)        
                test.ac[i] = 0;
        } else {            
            for (int i=0;i<27;i++)
                test.ac[i] = my_rand() % 2;
            if (test.dc == 0)
                test.ac[0] = 1;
        }

        switch (func) {
        case OMX_CHROMADC_2x2:
            cidx += gen_coeff_delta(coeffs+cidx, cmask, 4);
            break;

        case OMX_LUMADC_4x4:
            cidx += gen_coeff_delta(coeffs+cidx, cmask, 16);
            break;

        case OMX_LUMA_4x4:
            cidx += gen_coeff_delta(coeffs+cidx, cmask, 16);
            break;

        case PROCESS_LUMA_16x16:
            for (int i=0;i<16;i++)
                cidx += gen_coeff_delta(coeffs+cidx, cmask, 16);
            break;

        case PROCESS_CHROMA_8x8x2:
            if (test.ac[25])
                cidx += gen_coeff_delta(coeffs+cidx, cmask, 4);                
            if (test.ac[26])
                cidx += gen_coeff_delta(coeffs+cidx, cmask, 4);                
            for (int i=0;i<16;i++)
                cidx += gen_coeff_delta(coeffs+cidx, cmask, 16);
            break;
        }

        assert(cidx < sizeof(coeffs));
        data_cache_hit_writeback(coeffs, 1024);
        test.src = &coeffs[src_offset];

        if (func == OMX_CHROMADC_2x2 || func == OMX_LUMADC_4x4) {
            if (!dequant_dc_test(&test, verbose)) {
                printf("FAILED TEST: #%d\n", i);
                while(1) {}
            }
        } else {
            if (!dequant_test(&test, verbose)) {
                printf("FAILED TEST: #%d\n", i);
                while(1) {}
            }
        }
    }
    return true;
}

uint8_t* coeff_buf_decode(uint8_t *src, int16_t *dst) {
    uint8_t flg;
    do {
        flg = *src;
        if (flg & 0x10) {
            dst[flg & 0xF] = ((int16_t)src[2]<<8) | (int16_t)src[1];
            src += 3;
        } else {
            dst[flg & 0xF] = (int16_t)(int8_t)src[1];
            src += 2;
        }
    } while ((flg & 0x20) == 0);
    return src;
}

bool exhaustive_cavlc_test(int numtests, int verbose) {
    uint8_t in_buf[256];
    uint8_t out_buf1[sizeof(in_buf)];
    uint8_t out_buf2[sizeof(in_buf)];

    int16_t coeff1[16];
    int16_t coeff2[16];

    for (int nt=0;nt<numtests;nt++) {
        my_srand(2048+nt);

        for (int i=0;i<sizeof(in_buf);i++) {
            in_buf[i] = my_rand();
            out_buf1[i] = 0xFF;
            out_buf2[i] = 0xFF;
        }
        int bitOff1 = my_rand()%8;
        int bitOff2 = bitOff1;

        int maxNumCoeff;
        switch (my_rand()%3) {
        case 0: maxNumCoeff = 15; break;
        case 1: maxNumCoeff = 16; break;
        case 2: maxNumCoeff = 4; break;
        default: assert(0);
        }

        int nc = my_rand() % 16;

        const uint8_t *in1 = in_buf;
        const uint8_t *in2 = in_buf;
        uint8_t *out1 = out_buf1;
        uint8_t *out2 = out_buf2;
        uint8_t numCoeff1, numCoeff2;
        uint8_t totalZeroes1, totalZeroes2;
        int ok1, ok2;

        uint32_t ref_time = TIME_STATEMENT({
            extern uint8_t DEBUG_armVCM4P10_DecodeCoeffsToPair_LastTotalZeros;
            OMXResult err;
            if (maxNumCoeff == 4) {
                err = omxVCM4P10_DecodeChromaDcCoeffsToPairCAVLC(
                    &in1, &bitOff1, &numCoeff1, &out1
                );
            } else {            
                err = omxVCM4P10_DecodeCoeffsToPairCAVLC(
                    &in1, &bitOff1,
                    &numCoeff1, // output
                    &out1, nc, maxNumCoeff);
            }
            totalZeroes1 = DEBUG_armVCM4P10_DecodeCoeffsToPair_LastTotalZeros;
            ok1 = (err == OMX_Sts_NoErr);
        });

        rsph264_queue_debug_random_status();
        rsph264_queue_set_cavlc_buffer(0, in2, bitOff2);
        rsph264_queue_reset_packed_delta_buffer();
        rsph264_sync();
        uint32_t rsp_time = TIME_STATEMENT({
            int outlen;
            if (maxNumCoeff == 4) 
                rsph264_queue_decode_chromadc_coeffs_pair_cavlc(0);
            else
                rsph264_queue_decode_coeffs_pair_cavlc(0, nc, maxNumCoeff);
            ok2 = rsph264_DEBUG_cavlc(out2, &outlen, &numCoeff2, &totalZeroes2);
            out2 += outlen;
        });

        if (!ok1) {
            if (ok2) {
                printf("REF: error, RSP: ok\n");
                printf("FAILED TEST: #%d (maxCoeff: %d)\n", nt, maxNumCoeff);
                while(1) {}
                return false;
            }
            continue;
        }
        if (!ok2) {
            if (ok1) {
                printf("REF: OK, RSP: error\n");
                printf("FAILED TEST: #%d\n", nt);
                while(1) {}
                return false;                
            }
        }

        memset(coeff1, 0, sizeof(coeff1));
        memset(coeff2, 0, sizeof(coeff2));
        coeff_buf_decode(out_buf1, coeff1);
        coeff_buf_decode(out_buf2, coeff2);

        if (memcmp(coeff1, coeff2, sizeof(coeff1)) != 0) {
            printf("\n");
            printf("nt:%d Time: REF:%ld RSP:%ld\n", nt, ref_time, rsp_time);
            printf("nc: %d, max:%d\n", nc, maxNumCoeff);
            printf("TotalCoeffs: ref:%d, rsp: %d\n", numCoeff1, numCoeff2);
            printf("TotalZeroes: REF:%d RSP:%d\n", totalZeroes1, totalZeroes2);
            printf("CLen: ref:%d, rsp: %d\n", out1-out_buf1, out2-out_buf2);

            printf("ref:");
            for (int i=0;i<out1-out_buf1;i++)
                printf(" %02x", out_buf1[i]);
            printf("\n");
            printf("rsp:");
            for (int i=0;i<out2-out_buf2;i++)
                printf(" %02x", out_buf2[i]);
            printf("\n");

            printf("FAILED TEST: #%d\n", nt);
            while(1) {}
            return false;
        }

    }
    return true;
}

bool exhaustive_decoderesidual_test(int numtests, int verbose) {
    static uint8_t in_buf[768];
    static uint8_t out_buf1[768];
    static uint8_t out_buf2[768];
    uint8_t tcup[24];
    uint8_t tcleft[24];
    uint64_t total_rsp_time = 0, total_ref_time = 0;

    for (int nt=0;nt<numtests;nt++) {
        my_srand(2000+nt);

        for (int i=0;i<sizeof(in_buf);i++) {
            in_buf[i] = my_rand();
            out_buf1[i] = 0;
            out_buf2[i] = 0;
        }
        int bytePos = my_rand()%8;
        int bitPos = my_rand()%8;

        uint8_t *left = 0, *up = 0;
        if (my_rand()%2) {
            for (int i=0;i<24;i++)
                tcup[i] = my_rand()%16;
            up = tcup;
        }
        if (my_rand()%2) {
            for (int i=0;i<24;i++)
                tcleft[i] = my_rand()%16;
            left = tcleft;
        }

        uint8_t is16x16 = (my_rand()%4)==0 ? 1 : 0;
        uint8_t codedBlockPattern = my_rand() & 0x3F;

        const uint8_t *src1 = in_buf+bytePos;
        const uint8_t *src2 = in_buf+bytePos;
        int src1bit = bitPos;
        int src2bit = bitPos;
        uint8_t *dst1 = out_buf1;
        uint8_t totalCoeff1[32];
        uint8_t totalCoeff2[32];
        for (int i=0;i<32;i++) {
            totalCoeff1[i] = totalCoeff2[i] = 0xAB;
        }

        OMXResult err1;
        uint32_t ref_time = TIME_STATEMENT({            
            err1 = HIGHFUNC_DecodeResidual(
                &src1, &src1bit, &dst1, totalCoeff1,
                left, up, codedBlockPattern, is16x16
            );
        });

        if (err1 != OMX_Sts_NoErr) {
            continue;
        }

        rsph264_queue_debug_random_status();
        rsph264_sync();
        uint32_t rsp_time = TIME_STATEMENT({
            rsph264_queue_set_cavlc_buffer(0, src2, bitPos);
            rsph264_queue_reset_packed_delta_buffer();
            rsph264_queue_decode_residual(0, out_buf2, totalCoeff2,
                left, up, codedBlockPattern, is16x16);
            rsph264_sync();
            rsph264_cur_cavlc_buffer(&src2, &src2bit);
        });

        if (memcmp(totalCoeff1, totalCoeff2, 27) != 0) {
            if (verbose >= 1) {            
                printf("FAILED: %d: coded:%x 16x16:%d lu:%d%d\n", nt, codedBlockPattern, is16x16, left!=0, up!=0);
                printf("tc1: %x%x%x%x %x%x%x%x %x%x%x%x %x%x%x%x %x%x%x%x %x%x%x%x %x%x%x\n",
                    totalCoeff1[0],totalCoeff1[1],totalCoeff1[2],totalCoeff1[3],
                    totalCoeff1[4],totalCoeff1[5],totalCoeff1[6],totalCoeff1[7],
                    totalCoeff1[8],totalCoeff1[9],totalCoeff1[10],totalCoeff1[11],
                    totalCoeff1[12],totalCoeff1[13],totalCoeff1[14],totalCoeff1[15],
                    totalCoeff1[16],totalCoeff1[17],totalCoeff1[18],totalCoeff1[19],
                    totalCoeff1[20],totalCoeff1[21],totalCoeff1[22],totalCoeff1[23],
                    totalCoeff1[24],totalCoeff1[25],totalCoeff1[26]);
                printf("tc2: %x%x%x%x %x%x%x%x %x%x%x%x %x%x%x%x %x%x%x%x %x%x%x%x %x%x%x\n",
                    totalCoeff2[0],totalCoeff2[1],totalCoeff2[2],totalCoeff2[3],
                    totalCoeff2[4],totalCoeff2[5],totalCoeff2[6],totalCoeff2[7],
                    totalCoeff2[8],totalCoeff2[9],totalCoeff2[10],totalCoeff2[11],
                    totalCoeff2[12],totalCoeff2[13],totalCoeff2[14],totalCoeff2[15],
                    totalCoeff2[16],totalCoeff2[17],totalCoeff2[18],totalCoeff2[19],
                    totalCoeff2[20],totalCoeff2[21],totalCoeff2[22],totalCoeff2[23],
                    totalCoeff2[24],totalCoeff2[25],totalCoeff2[26]);
                return false;
            }
        }

        // Compare all coefficients
        int16_t coeff1[16];
        int16_t coeff2[16];
        uint8_t *cbuf1 = out_buf1, *cbuf2 = out_buf2;

        for (int i=0;i<27;i++) {
            if (totalCoeff1[i] != 0) {
                memset(coeff1, 0, 16*2);
                memset(coeff2, 0, 16*2);
                uint8_t *new1 = coeff_buf_decode(cbuf1, coeff1);
                uint8_t *new2 = coeff_buf_decode(cbuf2, coeff2);

                if (memcmp(coeff1, coeff2, sizeof(coeff1)) != 0) {
                    if (verbose >= 1) {                    
                        printf("FAILED: %d: tc match, but coeffs differ\n", nt);
                        printf("ref:");
                        for (int i=0;i<new1-cbuf1;i++)
                            printf(" %02x", cbuf1[i]);
                        printf("\n");
                        printf("rsp:");
                        for (int i=0;i<new2-cbuf2;i++)
                            printf(" %02x", cbuf2[i]);
                        printf("\n");
                    }
                    return false;
                }

                cbuf1 = new1;
                cbuf2 = new2;
            }
        }

        if (src1 != src2 || src1bit != src2bit) {
            if (verbose >= 1) {            
                printf("FAILED: %d: different bitstream position:\n", nt);
                printf("REF: %08lx[%d]\n", (uint32_t)src1, src1bit);
                printf("RSP: %08lx[%d]\n", (uint32_t)src2, src2bit);
                while(1);
            }
            return false;
        }

        total_ref_time += (uint64_t)ref_time;
        total_rsp_time += (uint64_t)rsp_time;

        if (verbose >= 3) {
            debugf("%d: coded:%x is16x16:%d lu:%d%d (%ld/%ld)\n", nt, codedBlockPattern, is16x16, left!=0, up!=0, ref_time, rsp_time);            
        }
    }

    if (verbose >= 2) {
        debugf("\nRSP: %lld, REF: %lld\n", total_rsp_time >> 16, total_ref_time >> 16);
    }
    return true;
}


int main(void)
{
    debug_init_isviewer();
    debug_init_usblog();

    timer_init();
    console_init();

    printf("H264 RSP decoder tests\n\n");

    my_srand(0);
    uint8_t *pSrc1 = (uint8_t*)malloc_uncached(SRC_PITCH*SRC_PITCH);
    uint8_t *pSrc2 = (uint8_t*)malloc_uncached(SRC_PITCH*SRC_PITCH);
    uint8_t *pDst1 = (uint8_t*)malloc_uncached(DST_SIZE*DST_SIZE);
    uint8_t *pDst2 = (uint8_t*)malloc_uncached(DST_SIZE*DST_SIZE);

    debugf("Buffers:\n");
    debugf(" pSrc1: %p - %p\n", pSrc1, pSrc1+SRC_PITCH*SRC_PITCH+16);
    debugf(" pSrc2: %p - %p\n", pSrc2, pSrc2+SRC_PITCH*SRC_PITCH+16);
    debugf(" pDst1: %p - %p\n", pDst1, pDst1+DST_SIZE*DST_SIZE+16);
    debugf(" pDst2: %p - %p\n", pDst2, pDst2+DST_SIZE*DST_SIZE+16);

    // Create random source buffer
    for (int y = 0; y < SRC_PITCH; y++) {
        for (int x = 0; x < SRC_PITCH; x++)
            pSrc1[y*SRC_PITCH+x] = pSrc2[y*SRC_PITCH+x] = my_rand();
    }

    const int OVERFILL = (SRC_PITCH-SRC_SIZE)/2;

    // RSP code automatically handles overfilling. To test it,
    // pre-overfill the source buffer used by the reference implementation.
    for (int y=OVERFILL;y<OVERFILL+SRC_SIZE;y++) {
        uint8_t *line = pSrc2+y*SRC_PITCH;
        memset(line, line[OVERFILL], OVERFILL);  // left 
        memset(line+OVERFILL+SRC_SIZE, line[OVERFILL+SRC_SIZE-1], OVERFILL); // right
    }
    for (int y=0;y<OVERFILL;y++)  // top
        memcpy(pSrc2+y*SRC_PITCH, pSrc2+OVERFILL*SRC_PITCH, SRC_PITCH);
    for (int y=OVERFILL+SRC_SIZE;y<OVERFILL+SRC_SIZE+OVERFILL;y++) // bottom
        memcpy(pSrc2+y*SRC_PITCH, pSrc2+(OVERFILL+SRC_SIZE-1)*SRC_PITCH, SRC_PITCH);

    // Skip overfilling
    pSrc1 += OVERFILL*SRC_PITCH + OVERFILL;
    pSrc2 += OVERFILL*SRC_PITCH + OVERFILL;

    // Create random destination buffer
    for (int y = 0; y < DST_SIZE; y++) {
        for (int x = 0; x < DST_SIZE; x++)
            pDst1[y*DST_SIZE+x] = pDst2[y*DST_SIZE+x] = my_rand();
    }

    rsph264_init();
    rsph264_begin_frame();

    // 0: nothing, 1: errors, 2: log, 3: uber-log
    int verbose = 1;

    BufferTest buftest;
    buftest.pSrc1 = pSrc1;
    buftest.pSrc2 = pSrc2;
    buftest.pDst1 = pDst1;
    buftest.pDst2 = pDst2;

    printf("OpenMAX VCM4P10:\n");
#if 0
    printf("OMX_DecodeCoeffsToPairCAVLC... "); fflush(stdout);
    exhaustive_cavlc_test(16*1024, verbose);
    printf("OK\n");
#endif
    printf("OMX_DequantTransformResidual... "); fflush(stdout);
    exhaustive_dequant_test(&buftest, OMX_LUMA_4x4, 4*1024, verbose);
    printf("OK\n");

    printf("OMX_TransformDequantLumaDC... "); fflush(stdout);
    exhaustive_dequant_test(&buftest, OMX_LUMADC_4x4, 4*1024, verbose);
    printf("OK\n");

    printf("OMX_TransformDequantChromaDC... "); fflush(stdout);
    exhaustive_dequant_test(&buftest, OMX_CHROMADC_2x2, 4*1024, verbose);
    printf("OK\n");

    InterpolationTest inttest;
    inttest.buf = buftest;

    printf("OMX_InterpolateLuma... "); fflush(stdout);
    inttest.func = INTERPOLATE_LUMA;
    overfill_interpolation_test(&inttest, verbose);
    exhaustive_interpolation_test(&inttest, 32, verbose);
    printf("OK\n");

    printf("OMX_InterpolateChroma... "); fflush(stdout);
    inttest.func = INTERPOLATE_CHROMA;
    overfill_interpolation_test(&inttest, verbose);
    exhaustive_interpolation_test(&inttest, 8, verbose);
    printf("OK\n");

    IntraPredictionTest intratest;
    intratest.buf = buftest;

    printf("OMX_IntraPredictLuma4x4... "); fflush(stdout);
    intratest.func = INTRAPRED_LUMA_4X4;
    exhaustive_intrapred_test(&intratest, 1*1024, verbose);
    printf("OK\n");

    printf("OMX_IntraPredictLuma16x16... "); fflush(stdout);
    intratest.func = INTRAPRED_LUMA_16X16;
    exhaustive_intrapred_test(&intratest, 2*1024, verbose);
    printf("OK\n");

    printf("OMX_IntraPredictChroma8x8... "); fflush(stdout);
    intratest.func = INTRAPRED_CHROMA_8X8;
    exhaustive_intrapred_test(&intratest, 2*1024, verbose);
    printf("OK\n");

    printf("\nHigh-level:\n");

#if 0
    printf("DecodeResidual..."); fflush(stdout);
    exhaustive_decoderesidual_test(16*1024, verbose);
    printf("OK\n");
#endif

    printf("ProcessLumaInterResidual... "); fflush(stdout);
    exhaustive_dequant_test(&buftest, PROCESS_LUMA_16x16, 2048, verbose);
    printf("OK\n");

    printf("ProcessChromaResidual... "); fflush(stdout);
    exhaustive_dequant_test(&buftest, PROCESS_CHROMA_8x8x2, 2048, verbose);
    printf("OK\n");

    printf("ProcessLumaIntra4x4Residual... "); fflush(stdout);
    intratest.func = INTRAPRED_PROCESS_LUMA4;
    exhaustive_intrapred_test(&intratest, 512, verbose);
    printf("OK\n");

    printf("ProcessLumaIntra16x16Residual.. "); fflush(stdout);
    intratest.func = INTRAPRED_PROCESS_LUMA16;
    exhaustive_intrapred_test(&intratest, 512, verbose);
    printf("OK\n");

    printf("\nALL TESTS PASSED\n");
}

