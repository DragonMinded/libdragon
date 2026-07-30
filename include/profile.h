/**
 * @file profile.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief CPU profiler API.
 */
#ifndef PROFILE_H
#define PROFILE_H

#include "n64sys.h"
#include "emux.h"
#include <stdint.h>

/** 
 * @brief Enable/disable of the profiler.
 * 
 * You can define this to 0 at compile-time if you want to keep PROFILE_*
 * calls in your code but otherwise ignore them.
 */
#ifndef LIBDRAGON_PROFILE
#define LIBDRAGON_PROFILE 1
#endif

// On PC, always disable the profiler.
#ifndef N64
#undef LIBDRAGON_PROFILE
#define LIBDRAGON_PROFILE 0
#endif

#include "n64sys.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Parameters for the profiler */
typedef struct {
	/**
	 * @brief Number of profiling slots (default: 128)
	 * 
	 * This is the number of different categories that can be
	 * used to profile code sections.
	 */
	int num_slots;

	/**
	 * @brief Interval in seconds to dump profiling info to stderr (default: 5.0s)
	 * 
	 * Use a negative number to disable periodic dumps. Notice that each time
	 * am automatic dump is performed at this interval, the profiler is also reset.
	 */
	float dump_stderr_interval;
} profile_parms_t;

/**
 * @brief Initialize the profiler.
 *
 * Initialize the profiler. This must be called before using any of the
 * PROFILE_* macros.
 * 
 * @param parms 		Parameters for the profiler (NULL to use defaults)
 * 
 * @see #profile_parms_t
 */
void profile_init(profile_parms_t *parms);

/**
 * @brief Close the profiler.
 */
void profile_close(void);

/**
 * @brief Reset the profiler.
 * 
 * Reset the profiler. This will clear all accumulated profile data and reset
 * the counters to 0.
 *
 * By default, this is called automatically at the interval specified by
 * profile_parms_t::dump_stderr_interval.
 *
 * @see #profile_parms_t
 * @see #profile_init
 */
void profile_reset(void);

/**
 * @brief Register a new profile slot
 * 
 * This function register a slot to be used with the PROFILE_* macros. It allows
 * to specify the name with which the slot will be identified in the dumps.
 * 
 * @param slot 			Slot number to register
 * @param name 			Name of the slot
 * @param nest_level 	Number of nesting levels for this slot
 *
 * @see #profile_parms_t
 * @see #profile_init
 */
void profile_register(int slot, const char *name, int nest_level);

/**
 * @brief Communicate to the profiler the target frame rate
 *
 * This is an optional function that can be called to communicate to the
 * profiler the target frame rate. When the profiler is aware of the target
 * frame rate, it will be used to show stats related to the expected deadline for
 * each frame (eg: how much CPU percentage is used on average in each frame).
 +
 * @param fps 			Target frame rate in frames per second
 */
void profile_set_target_fps(float fps);

/**
 * @brief Mark the start of the next frame
 * 
 * This function must be called at the end of each frame. It will accumulate the
 * profile data for the current frame and prepare the profiler for the next frame.
 *
 * This function will also trigger a dump and reset of the profiler data if the
 * dump interval has been reached (see profile_parms_t::dump_stderr_interval).
 *
 * It is not mandatory to call this function at all; if you want to profile
 * individual code sections not related to the specific concept of a frame loop,
 * you can simply use the PROFILE_* macros as you please and then call
 * profile_dump() to obtain the results.
 *
 * @see #profile_parms_t
 * @see #profile_dump
 */
void profile_next_frame(void);

/**
 * @brief Dump the profiler data to the console
 * 
 * This function will dump the current profiler data to the debugging channel.
 *
 * Notice that profile data is not reset after dumping, so it is possible to call
 * this function multiple times and see data being accumulated over time. If you
 * want to reset the profiler data after dumping, you can call profile_reset()
 * after calling profile_dump().
 *
 * @see #profile_reset
 */
void profile_dump(void);

///@cond
// This function is *NOT* part of the public API
inline void __profile_record(int slot, int32_t len) {
	extern uint64_t *__profile_counters;
	if (__profile_counters) __profile_counters[slot] += ((int64_t)len << 32) + 1;
}
///@endcond

#if LIBDRAGON_PROFILE
	#include "pputils.h"

	// Internal helpers to make the ordinal parameter optional in PROFILE_START/STOP.
	// If omitted, it defaults to 0 (so PROFILE_START(SLOT) pairs with PROFILE_STOP(SLOT)).
	#define __PROFILE_START_0(slot) \
		int64_t __prof_start_##slot##_0 = TICKS_READ() - get_system_ticks(); \
		emux_prof_start(slot); \
		MEMORY_BARRIER();
	#define __PROFILE_START_1(slot, n) \
		int64_t __prof_start_##slot##_##n = TICKS_READ() - get_system_ticks(); \
		emux_prof_start(slot); \
		MEMORY_BARRIER();
	#define __PROFILE_STOP_0(slot) \
		MEMORY_BARRIER(); \
		emux_prof_stop(slot); \
		__profile_record(slot, TICKS_READ() - get_system_ticks() - __prof_start_##slot##_0);
	#define __PROFILE_STOP_1(slot, n) \
		MEMORY_BARRIER(); \
		emux_prof_stop(slot); \
		__profile_record(slot, TICKS_READ() - get_system_ticks() - __prof_start_##slot##_##n);
	/** 
	 * @brief Start profiling a code section
	 *
	 * This macro is used to start profiling a code section, selecting the slot
	 * in which to record the profile data:
	 *
	 * \begin{code}
	 * 		PROFILE_START(PHYSICS)
	 * 		phys_update(delta_time);
	 * 		PROFILE_STOP(PHYSICS)
	 * \end{code}
	 *
	 * A second optional parameter must be used with an ordinal number to allow
	 * multiple code sections with the same slot to be profiled within the same
	 * lexical scope.
	 *
	 * \begin{code}
	 * 		PROFILE_START(PHYSICS, 1)
	 * 		phys_update_first_pass(delta_time);
	 * 		PROFILE_STOP(PHYSICS, 1)
	 *
	 *      update_something_else();
	 *
	 * 		PROFILE_START(PHYSICS, 2)
	 * 		phys_update_second_pass(delta_time);
	 * 		PROFILE_STOP(PHYSICS, 2)
	 * \end{code}
	 *
	 * @param slot 			Slot to profile
	 * @param n 			Optional ordinal number for multiple profile sections
	 *                      within the same lexical scope
	 *
	 * @see PROFILE_STOP
	 * @see PROFILE_SCOPE
	 */
	#define PROFILE_START(slot, ...) \
		__PPCAT(__PROFILE_START_, __HAS_VARARGS(__VA_ARGS__))(slot, ##__VA_ARGS__)

	/**
	 * @brief Stop profiling a code section
	 * 
	 * This macro is used to stop profiling a code section. It must match
	 * the PROFILE_START macro call for the same slot and same optional
	 * ordinal number.
	 *
	 * @param slot 			Slot to profile
	 * @param n 			Optional ordinal number for multiple profile sections
	 *                      within the same lexical scope
	 *
	 * @see PROFILE_START
	 * @see PROFILE_SCOPE
	 */
	#define PROFILE_STOP(slot, ...) \
		__PPCAT(__PROFILE_STOP_, __HAS_VARARGS(__VA_ARGS__))(slot, ##__VA_ARGS__)

	/**
	 * @brief Start a scoped profiling section
	 * 
	 * This macro is used to start a scoped profiling section. It will start
	 * a profiling section for the given slot and will stop it when the
	 * scope is exited.
	 *
	 * \begin{code}
	 * 		PROFILE_SCOPE(PHYSICS) {
	 * 			phys_update(delta_time);
	 * 		}
	 * \end{code}
	 *
	 * Notice that this macros never requires an ordinal number like #PROFILE_START,
	 * because it is not possible to have multiple code sections with the same slot
	 * within the same lexical scope.
	 *
	 * @param slot 			Slot to profile
	 *
	 * @see PROFILE_START
	 * @see PROFILE_STOP
	 */
	#define PROFILE_SCOPE(slot) \
		for (int64_t __prof_start_##slot = emux_prof_start(slot), TICKS_READ() - get_system_ticks() ; ) \
			for (int __prof_once_##slot = ({ MEMORY_BARRIER(); 1; }); __prof_once_##slot; __prof_once_##slot = 0, \
				({ MEMORY_BARRIER(); }), \
				emux_prof_stop(slot), \
				__profile_record(slot, TICKS_READ() - get_system_ticks() - __prof_start_##slot))
#else
	///@cond
	#define PROFILE_START(slot, ...)  ((void)(false), false)
	#define PROFILE_STOP(slot, ...)   ((void)(false), false)
	#define PROFILE_SCOPE(slot)     for (bool __prof_once_##slot = true; __prof_once_##slot; __prof_once_##slot = false)
	///@endcond

#endif /* LIBDRAGON_PROFILE */

#ifdef __cplusplus
}
#endif

#endif /* PROFILE_H */
