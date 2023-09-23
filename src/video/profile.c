#include "profile.h"
#include "debug.h"
#include "n64sys.h"
#include "timer.h"
#include <memory.h>
#include <stdio.h>

#define SCALE_RESULTS  2048

uint64_t slot_total[PS_NUM_SLOTS];
uint64_t slot_total_count[PS_NUM_SLOTS];
uint64_t total_time;
uint64_t last_frame;
uint64_t slot_frame_cur[PS_NUM_SLOTS];
int frames;

void profile_init(void) {
	memset(slot_total, 0, sizeof(slot_total));
	memset(slot_total_count, 0, sizeof(slot_total_count));
	memset(slot_frame_cur, 0, sizeof(slot_frame_cur));
	frames = 0;

	total_time = 0;
	last_frame = TICKS_READ();
}

void profile_next_frame(void) {
	for (int i=0;i<PS_NUM_SLOTS;i++) {
		// Extract and save the total time for this frame.
		slot_total[i] += slot_frame_cur[i] >> 32;
		slot_total_count[i] += slot_frame_cur[i] & 0xFFFFFFFF;
		slot_frame_cur[i] = 0;
	}
	frames++;

	// Increment total profile time. Make sure to handle overflow of the
	// hardware profile counter, as it happens frequently.
	uint64_t count = TICKS_READ();
	total_time += TICKS_DISTANCE(last_frame, count);
	last_frame = count;
}

static void stats(ProfileSlot slot, uint64_t frame_avg, uint32_t *mean, float *partial) {
	*mean = slot_total[slot]/frames;
	*partial = (float)*mean * 100.0 / (float)frame_avg;
}

void profile_dump(void) {
	debugf("%-14s %4s %6s %6s\n", "Slot", "Cnt", "Avg", "Perc");
	debugf("----------------------------------\n");

	uint64_t frame_avg = total_time / frames;
	char buf[64];

#define DUMP_SLOT(slot, name) ({ \
	uint32_t mean; float partial; \
	stats(slot, frame_avg, &mean, &partial); \
	sprintf(buf, "%2.1f", partial); \
	debugf("%-25s %4llu %6d %5s%%\n", name, \
		 slot_total_count[slot] / frames, \
		 TIMER_MICROS(mean), \
		 buf); \
})

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

	debugf("----------------------------------\n");
	debugf("Profiled frames:      %4d\n", frames);
	debugf("Frames per second:    %4.1f\n", (float)TICKS_PER_SECOND/(float)frame_avg);
	debugf("Average frame time:   %4d\n", TIMER_MICROS(frame_avg));
	debugf("Target frame time:    %4d\n", TIMER_MICROS(TICKS_PER_SECOND/45));
}
