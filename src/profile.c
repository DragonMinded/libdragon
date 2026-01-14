/**
 * @file profile.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief CPU profiler implementation.
 */
#include "profile.h"
#include "debug.h"
#include "n64sys.h"
#include "timer.h"
#include "accounting_internal.h"
#include <memory.h>
#include <stdio.h>

/** @brief Internal profile slot data */
typedef struct {
	const char *name;		///< Name of the slot
	int nest_level;			///< Number of nesting levels for this slot
	uint32_t ticks;			///< Total number of ticks spent in the slot
	uint32_t cache_misses; 	///< Total number of cache misses in the slot
	uint32_t cache_hits; 	///< Total number of cache hits in the slot
	int count;				///< Number of times the slot was hit
} profile_slot_t;

/** @brief Profile counters (actual accumulators) */
uint64_t *__profile_counters;
/** @brief Porfile slots descriptions */
profile_slot_t *slots;
/** @brief Number of profile slots */
static int num_slots;

static uint64_t total_time_wall;
static uint64_t total_time_user;
static uint64_t last_frame_wall;
static uint64_t last_frame_user;
static uint64_t target_frame_ticks;
static uint64_t sys_total[ACCT_CAT_MAX];
static uint64_t sys_frame_last[ACCT_CAT_MAX];
static int frames;
static uint64_t dump_interval;
static uint64_t last_dump_time;

void profile_init(profile_parms_t *parms) {
	num_slots = (parms && parms->num_slots > 0) ? parms->num_slots : 128;
	slots = calloc(num_slots, sizeof(profile_slot_t));
	__profile_counters = calloc(num_slots, sizeof(uint64_t));
	assertf(slots && __profile_counters, "Out of memory");

	float dump_time = parms ? parms->dump_stderr_interval : 5.0f;
	if (dump_time >= 0.0f) {
		dump_interval = (uint64_t)(dump_time * TICKS_PER_SECOND);
	} else {
		dump_interval = 0;
	}
	last_dump_time = get_ticks();

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
		const char *name = slots[i].name;
		int nest_level = slots[i].nest_level;
		sys_hw_memset(&slots[i], 0, sizeof(profile_slot_t));
		slots[i].name = name;
		slots[i].nest_level = nest_level;
	}
	frames = 0;
	total_time_wall = 0;
	total_time_user = 0;
	last_frame_wall = get_ticks();
	last_frame_user = get_user_ticks();

	bool emux_profile_available = (emux_detect() & EMUX_FEAT_PROFILER) != 0;
	for (int i=0; i<ACCT_CAT_MAX; i++) {
		sys_total[i] = 0;
		if (i == ACCT_CAT_IRQ && emux_profile_available)
			sys_frame_last[i] = emux_prof_read(-1, EMUX_PROF_CYCLES_EXC);
		else
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
	bool emux_profile_available = (emux_detect() & EMUX_FEAT_PROFILER) != 0;
	for (int i=0; i<num_slots; i++) {
		profile_slot_t *slot = &slots[i];
		// Extract and save the total time for this frame.
		slot->ticks += __profile_counters[i] >> 32;
		slot->count += __profile_counters[i] & 0xFFFFFFFF;
		if (emux_profile_available) {
			slot->cache_misses += emux_prof_read(i, EMUX_PROF_ICACHE_MISSES) + emux_prof_read(i, EMUX_PROF_DCACHE_MISSES);
			slot->cache_hits   += emux_prof_read(i, EMUX_PROF_ICACHE_HITS)   + emux_prof_read(i, EMUX_PROF_DCACHE_HITS);
		}
		__profile_counters[i] = 0;
		emux_prof_clear(i);
	}
	for (int i=0;i<ACCT_CAT_MAX;i++) {
		uint64_t now;
		if (i == ACCT_CAT_IRQ) {
			// For the IRQ category, we use emux if available since accounting.h
			// still doesn't implement it properly (it's hard to do without
			// slowing down the interrupt handler).
			now = emux_prof_read(-1, EMUX_PROF_CYCLES_EXC);
		} else {
			now = acct_get_ticks(i);
		}
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

	// Check if we need to emit a dump
	if (dump_interval && last_dump_time + dump_interval <= wall_count) {
		profile_dump();
		profile_reset();
		last_dump_time = wall_count;
	}
}

void profile_dump(void) {
	debugf("%-35s %4s    %-15s %-10s\n", "Slot", "Cnt", "Avg", "Cache Misses");
	debugf("------------------------------------------------------------------------\n");

	uint64_t frame_avg_user = total_time_user / frames;
	uint64_t frame_avg_wall = total_time_wall / frames;
	float partial_sys;

	for (int i=0; i<num_slots; i++) {
		if (slots[i].name == NULL) continue;

		char name[128]; char *n = name;
		for (int j=0;j<slots[i].nest_level;j++) {
			*n++ = ' '; *n++ = ' ';
		}
		*n++ = '-'; *n++ = ' ';
		strcpy(n, slots[i].name);
		
		uint32_t mean = slots[i].ticks / frames;
	    float partial_avg = (float)mean * 100.0 / (float)frame_avg_wall;
		int cache_misses = slots[i].cache_misses / frames;
		float cache_miss_rate = 0.0f;
		if (slots[i].cache_hits + slots[i].cache_misses > 0) {
			cache_miss_rate = (float)cache_misses * 100.0f /
				(float)(slots[i].cache_hits + slots[i].cache_misses);
		}

		int avg_us = TIMER_MICROS(mean);
		debugf("%-35.35s %4d %6d (%5.1f%%) %6d (%5.2f%%)\n",
			name, slots[i].count / frames, avg_us, partial_avg, cache_misses, cache_miss_rate);
	}

///@cond
#define DUMP_SYS(cat, name) ({ \
	uint64_t ticks = sys_total[cat] / frames; \
	partial_sys = (float)ticks * 100.0f / (float)frame_avg_wall; \
	if (ticks) debugf("%-35.35s %4s %6d (%5.1f%%)\n", name, "-", \
					  TIMER_MICROS(ticks), partial_sys); \
})
///@endcond

	DUMP_SYS(ACCT_CAT_IRQ, "[sys] IRQ time");
	DUMP_SYS(ACCT_CAT_RSP, "[sys] RSP wait");
	DUMP_SYS(ACCT_CAT_DISPLAY, "[sys] Display wait");
	DUMP_SYS(ACCT_CAT_RSPQ, "[sys] RSPQ wait");
	DUMP_SYS(ACCT_CAT_VI, "[sys] VI wait");
	DUMP_SYS(ACCT_CAT_JOYBUS, "[sys] Joybus wait");

	debugf("------------------------------------------------------------------------\n");
	debugf("Profiled frames:      %4d\n", frames);
	debugf("Frames per second:    %4.1f\n", (float)TICKS_PER_SECOND/(float)frame_avg_wall);
	debugf("Target frame time:    %4d us\n", TIMER_MICROS(target_frame_ticks));
	debugf("Average frame (wall): %4d us\n", TIMER_MICROS(frame_avg_wall));
	debugf("Average frame (user): %4d us (%2.1f%%)\n", TIMER_MICROS(frame_avg_user), 
		(float)frame_avg_user * 100.0f / (float)frame_avg_wall);
	debugf("Average frame (sys):  %4d us (%2.1f%%)\n", TIMER_MICROS(frame_avg_wall - frame_avg_user), 
		(float)(frame_avg_wall - frame_avg_user) * 100.0f / (float)frame_avg_wall);
}
