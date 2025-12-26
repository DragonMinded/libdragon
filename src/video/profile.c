/**
 * @file profile.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#include "profile.h"
#include "debug.h"
#include "n64sys.h"
#include "timer.h"
#include "accounting_internal.h"
#include <memory.h>
#include <stdio.h>

static uint64_t slot_total[PS_NUM_SLOTS];
static uint64_t slot_total_count[PS_NUM_SLOTS];
static uint64_t sys_total[ACCT_CAT_MAX];
static uint64_t total_time_wall;
static uint64_t total_time_user;
static uint64_t last_frame_wall;
static uint64_t last_frame_user;
static uint64_t target_frame_ticks;
uint64_t slot_frame_cur[PS_NUM_SLOTS];
uint64_t sys_frame_last[ACCT_CAT_MAX];
static int frames;

void profile_init(void) {
	memset(slot_total, 0, sizeof(slot_total));
	memset(slot_total_count, 0, sizeof(slot_total_count));
	memset(slot_frame_cur, 0, sizeof(slot_frame_cur));
	for (int i=0; i<ACCT_CAT_MAX; i++) {
		sys_total[i] = 0;
		sys_frame_last[i] = acct_get_ticks(i);
	}

	frames = 0;
	total_time_wall = 0;
	total_time_user = 0;
	last_frame_wall = get_ticks();
	last_frame_user = get_user_ticks();
}

void profile_set_target_fps(float fps) {
	if (fps <= 0.0f) {
		target_frame_ticks = 0;
	} else {
		target_frame_ticks = (uint64_t)(TICKS_PER_SECOND / fps);
	}
}

void profile_next_frame(void) {
	for (int i=0;i<PS_NUM_SLOTS;i++) {
		// Extract and save the total time for this frame.
		slot_total[i] += slot_frame_cur[i] >> 32;
		slot_total_count[i] += slot_frame_cur[i] & 0xFFFFFFFF;
		slot_frame_cur[i] = 0;
	}
	for (int i=0;i<ACCT_CAT_MAX;i++) {
		uint64_t now = acct_get_ticks(i);
		sys_total[i] = now - sys_frame_last[i];
		sys_frame_last[i] = now;
	}
	frames++;

	// Increment total profile time. Make sure to handle overflow of the
	// hardware profile counter, as it happens frequently.
	uint64_t wall_count = get_ticks();
	uint64_t user_count = get_user_ticks();
	total_time_wall += wall_count - last_frame_wall;
	total_time_user += user_count - last_frame_user;
	last_frame_wall = wall_count;
	last_frame_user = user_count;
}

static void stats(ProfileSlot slot, uint64_t frame_avg, uint32_t *mean, float *partial) {
	*mean = slot_total[slot]/frames;
	*partial = (float)*mean * 100.0 / (float)frame_avg;
}

void profile_dump(void) {
	debugf("%-35s %4s %6s %6s\n", "Slot", "Cnt", "Avg", "Perc");
	debugf("---------------------------------------------------------\n");

	uint64_t frame_avg_user = total_time_user / frames;
	uint64_t frame_avg_wall = total_time_wall / frames;
	char buf[64];

#define DUMP_SLOT(slot, name) ({ \
	uint32_t mean; float partial; \
	stats(slot, frame_avg_user, &mean, &partial); \
	sprintf(buf, "%2.1f", partial); \
	if (slot_total_count[slot] > 0) \
		debugf("%-35s %4llu %6d %5s%%\n", name, \
			 slot_total_count[slot] / frames, \
		 	TIMER_MICROS(mean), \
		 	buf); \
})

#define DUMP_SYS(cat, name) ({ \
	uint64_t ticks = sys_total[cat]; \
	sprintf(buf, "%2.1f", (float)ticks * 100.0f / (float)frame_avg_wall); \
	debugf("%-35s    - %6d %5s%%\n", name, \
		TIMER_MICROS(ticks), \
		buf); \
})

	DUMP_SLOT(PS_H264, "H264");
	DUMP_SLOT(PS_H264_NAL, "  - NAL");
	DUMP_SLOT(PS_H264_MACROB, "  - MacroB");
	DUMP_SLOT(PS_H264_LAYER, "    - Layer");
	DUMP_SLOT(PS_H264_LAYER_CLEAR, "      - Clear");
	DUMP_SLOT(PS_H264_LAYER_PRED, "      - Predict");
	DUMP_SLOT(PS_H264_LAYER_RES, "      - Residual");
	DUMP_SLOT(PS_H264_LAYER_RES_ENC, "        - Encode");
	DUMP_SLOT(PS_H264_RESIDUAL_LUMA, "        - Residual Luma");
	DUMP_SLOT(PS_H264_RESIDUAL_CHROMA, "        - Residual Chroma");
	DUMP_SLOT(PS_H264_INTRAPRED_4X4, "          - IntraPred 4x4");
	DUMP_SLOT(PS_H264_INTRAPRED_16X16, "          - IntraPred 16x16");
	DUMP_SLOT(PS_H264_INTERPRED, "  - InterPred");
	DUMP_SLOT(PS_H264_INTERPRED_LUMA, "    - InterPred Luma");
	DUMP_SLOT(PS_H264_INTERPRED_CHROMA, "    - InterPred Chroma");
	DUMP_SLOT(PS_H264_SYNC, "  - Sync");
	DUMP_SLOT(PS_H264_SYNC_OVL, "    - Sync Overlay");

	DUMP_SLOT(PS_MPEG, "MPEG1");
	DUMP_SLOT(PS_MPEG_FINDSTART, "  - FindStart");
	DUMP_SLOT(PS_MPEG_HASSTART, "  - HasStart");
	DUMP_SLOT(PS_MPEG_DECODESLICE, "  - Slice");
	DUMP_SLOT(PS_MPEG_MB, "    - MacroB");
	DUMP_SLOT(PS_MPEG_MB_MV, "      - MV");
	DUMP_SLOT(PS_MPEG_MB_PREDICT, "      - Predict");
	DUMP_SLOT(PS_MPEG_MB_DECODE, "      - Decode");
	DUMP_SLOT(PS_MPEG_MB_DECODE_DC, "        - DC");
	DUMP_SLOT(PS_MPEG_MB_DECODE_AC, "        - AC");
	DUMP_SLOT(PS_MPEG_MB_DECODE_AC_VLC, "          - VLC");
	DUMP_SLOT(PS_MPEG_MB_DECODE_AC_CODE, "          - Code");
	DUMP_SLOT(PS_MPEG_MB_DECODE_AC_DEQUANT, "          - Dequant");
	DUMP_SLOT(PS_MPEG_MB_DECODE_BLOCK, "        - Block");
	DUMP_SLOT(PS_MPEG_MB_DECODE_BLOCK_IDCT, "          - IDCT");
	DUMP_SLOT(PS_YUV, "YUV Blit");
	DUMP_SLOT(PS_AUDIO, "Audio");
	DUMP_SLOT(PS_SYNC, "Sync");

	DUMP_SYS(ACCT_CAT_IRQ, "[sys] IRQ time");
	DUMP_SYS(ACCT_CAT_RSP, "[sys] RSP wait");
	DUMP_SYS(ACCT_CAT_DISPLAY, "[sys] Display wait");
	DUMP_SYS(ACCT_CAT_RSPQ, "[sys] RSPQ wait");
	DUMP_SYS(ACCT_CAT_VI, "[sys] VI wait");
	DUMP_SYS(ACCT_CAT_JOYBUS, "[sys] Joybus wait");

	debugf("---------------------------------------------------------\n");
	debugf("Profiled frames:      %4d\n", frames);
	debugf("Frames per second:    %4.1f\n", (float)TICKS_PER_SECOND/(float)frame_avg_wall);
	debugf("Target frame time:    %4d us\n", TIMER_MICROS(target_frame_ticks));
	debugf("Average frame (wall): %4d us\n", TIMER_MICROS(frame_avg_wall));
	debugf("Average frame (user): %4d us\n", TIMER_MICROS(frame_avg_user));
	debugf("Average frame (sys):  %4d us\n", TIMER_MICROS(frame_avg_wall - frame_avg_user));


}
