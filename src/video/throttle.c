#include "throttle.h"
#include "n64sys.h"
#include "timer.h"
#include <memory.h>

#if 0
	#include "debug.h"
    #define LOGF(fmt, ...) debugf(fmt, ##__VA_ARGS__)
#else
	#define LOGF(fmt, ...) do {} while(0)
#endif

struct {
	int64_t clock_fx16;
	int64_t ticks_per_frame_fx16;
	int can_frameskip;
	int frames_advance;
} Throttle;

void throttle_init(float fps, int can_frameskip, int frames_advance) {
	memset(&Throttle, 0, sizeof(Throttle));
	Throttle.can_frameskip = can_frameskip;
	Throttle.frames_advance = frames_advance;
	Throttle.ticks_per_frame_fx16 = ((int64_t)TICKS_PER_SECOND << 16) / fps;
	Throttle.clock_fx16 = (int64_t)TICKS_READ() << 16;
}

int throttle_wait(void) {
	uint32_t prev = (uint32_t)(Throttle.clock_fx16>>16); (void)prev;
	Throttle.clock_fx16 += Throttle.ticks_per_frame_fx16;
	uint32_t next = (uint32_t)(Throttle.clock_fx16>>16);

	uint32_t now = TICKS_READ();
	LOGF("throttle: prev:%lu now:%lu next:%lu (tpf:%lld)\n", prev/1024, now/1024, next/1024, 
		(Throttle.ticks_per_frame_fx16>>16)/1024);

	if (!TICKS_BEFORE(now, next)) {
		// We're coming late to this frame, it took too long to process.
		LOGF("throttle: frame too slow (%lu us)\n", TIMER_MICROS(TICKS_DISTANCE(prev, now)));

		// If the application cannot frameskip, reset the clock to
		// the current time, so that we allow a full time slice for next frame.
		if (!Throttle.can_frameskip)
			Throttle.clock_fx16 = (int64_t)now << 16;

		return -TIMER_MICROS(TICKS_DISTANCE(prev, now));
	}

	// We are on time for the current frame. See if we need to throttle
	// depending on how many frames we're allowed to be in advance.
	uint32_t target = (uint32_t)((Throttle.clock_fx16 - Throttle.ticks_per_frame_fx16*Throttle.frames_advance) >> 16);

	if (TICKS_BEFORE(now, target)) {	
		LOGF("throttle: waiting %ld Kcycles\n", TICKS_DISTANCE(now, target)/1024);
		while (TICKS_BEFORE(now, target))
			now = TICKS_READ();
	}

	return 0;
}

uint32_t throttle_frame_length(void) {
	return Throttle.ticks_per_frame_fx16 >> 16;
}

int32_t throttle_frame_time_left(void) {
	uint32_t next = (Throttle.clock_fx16 + Throttle.ticks_per_frame_fx16) >> 16;
	return TICKS_DISTANCE(TICKS_READ(), next);
}
