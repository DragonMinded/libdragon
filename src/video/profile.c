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

typedef struct {
	const char *name;
	uint32_t ticks;
	int count;
	int nest_level;
} profile_slot_t;

uint64_t *__profile_counters;
profile_slot_t *slots;
static int num_slots;
static uint64_t total_time_wall;
static uint64_t total_time_user;
static uint64_t last_frame_wall;
static uint64_t last_frame_user;
static uint64_t target_frame_ticks;
static uint64_t sys_total[ACCT_CAT_MAX];
uint64_t sys_frame_last[ACCT_CAT_MAX];
static int frames;

void profile_init(int ns) {
	slots = calloc(ns, sizeof(profile_slot_t));
	assertf(slots, "Out of memory");
	__profile_counters = calloc(ns, sizeof(uint64_t));
	assertf(__profile_counters, "Out of memory");
	num_slots = ns;

	profile_reset();
}

void profile_close(void)
{
	if (__profile_counters) {
		free(__profile_counters);
		__profile_counters = NULL;
	}
	if (slots) {
		free(slots);
		slots = NULL;
	}
	num_slots = 0;
}

void profile_reset(void) {
	sys_hw_memset(__profile_counters, 0, num_slots * sizeof(uint64_t));
	for (int i=0; i<num_slots; i++) {
		slots[i].ticks = 0;
		slots[i].count = 0;
	}
	frames = 0;
	total_time_wall = 0;
	total_time_user = 0;
	last_frame_wall = get_ticks();
	last_frame_user = get_user_ticks();
	for (int i=0; i<ACCT_CAT_MAX; i++) {
		sys_total[i] = 0;
		sys_frame_last[i] = acct_get_ticks(i);
	}
}

void profile_register(int slot, const char *name, int nest_level) {
	assertf(slot >= 0 && slot < num_slots, "Invalid profile slot %d", slot);
	slots[slot].name = name;
	slots[slot].nest_level = nest_level;
}

void profile_set_target_fps(float fps) {
	if (fps <= 0.0f) {
		target_frame_ticks = 0;
	} else {
		target_frame_ticks = (uint64_t)(TICKS_PER_SECOND / fps);
	}
}

void profile_next_frame(void) {
	for (int i=0; i<num_slots; i++) {
		// Extract and save the total time for this frame.
		slots[i].ticks += __profile_counters[i] >> 32;
		slots[i].count += __profile_counters[i] & 0xFFFFFFFF;
		__profile_counters[i] = 0;
	}
	for (int i=0;i<ACCT_CAT_MAX;i++) {
		uint64_t now = acct_get_ticks(i);
		sys_total[i] += now - sys_frame_last[i];
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

static void stats(profile_slot_t* slot, uint64_t frame_avg, uint32_t *mean, float *partial) {
	*mean = slot->ticks / frames;
	*partial = (float)*mean * 100.0 / (float)frame_avg;
}

void profile_dump(void) {
	debugf("%-35s %4s %6s %6s\n", "Slot", "Cnt", "Avg", "Perc");
	debugf("---------------------------------------------------------\n");

	uint64_t frame_avg_user = total_time_user / frames;
	uint64_t frame_avg_wall = total_time_wall / frames;
	char buf[64];

	for (int i=0; i<num_slots; i++) {
		if (slots[i].name == NULL) continue;
		if (slots[i].count == 0) continue;

		char name[128]; char *n = name;
		for (int j=0;j<slots[i].nest_level;j++) {
			*n++ = ' '; *n++ = ' ';
		}
		*n++ = '-'; *n++ = ' ';
		strcpy(n, slots[i].name);
		
		uint32_t mean; float partial;
		stats(&slots[i], frame_avg_wall, &mean, &partial);
		sprintf(buf, "%2.1f", partial);
		debugf("%-35s %4d %6d %5s%%\n",
			name, slots[i].count / frames,
			TIMER_MICROS(mean), buf);
	}

#define DUMP_SYS(cat, name) ({ \
	uint64_t ticks = sys_total[cat] / frames; \
	sprintf(buf, "%2.1f", (float)ticks * 100.0f / (float)frame_avg_wall); \
	if (ticks) debugf("%-35s    - %6d %5s%%\n", name, \
					  TIMER_MICROS(ticks), buf); \
})

#if 0
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
#endif

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
	debugf("Average frame (user): %4d us (%2.1f%%)\n", TIMER_MICROS(frame_avg_user), 
		(float)frame_avg_user * 100.0f / (float)frame_avg_wall);
	debugf("Average frame (sys):  %4d us (%2.1f%%)\n", TIMER_MICROS(frame_avg_wall - frame_avg_user), 
		(float)(frame_avg_wall - frame_avg_user) * 100.0f / (float)frame_avg_wall);
}
