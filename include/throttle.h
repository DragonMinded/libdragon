/* 
 * Throttle: helper for speed throttling.
 *
 * These functions provide helpers to do precise throttling
 * of the speed of the main loop of an application.
 *
 * They are specifically useful when it is not possible to do
 * sync with a hardware event (like the vertical blank) but it
 * is still required to slow down precisely the application
 * to a certain number of (even fractional) frames per second.
 */

#ifndef THROTTLE_H
#define THROTTLE_H

#include <stdint.h>

// Initialize the throttling engine, specifying the exact
// number of frames per second that we want to achieve.
// 
// "can_frameskip" should be non-zero if the application is able
// to do "frame skipping" to recover a delay in the frame rate.
// If this is not possible, then pass 0. 
//
// "frames_advance" is the number of frames that we want to allow
// being faster than expected before actual throttling. For instance,
// if a frame completed in 70% of the total time, one might want
// not to waste the 30% spin-waiting, but begin processing the next
// frame right away, in case it takes longer. A typical value for
// this parameter is "1", but you can experiment with different
// values depending on your constraints.
void throttle_init(float fps, int can_frameskip, int frames_advance);

// Throttle the CPU (spin-wait) to delay and achieve the specified
// number of frames per second. 
//
// The function returns 1 if everything is going well (that is,
// we're within the frames_advance allowance, or the CPU was
// throttled), or 0 when the function was called too late, that is
// the current frame has ran too long compared to expected pacing.
// If 0 is returned, the application might want to perform
// frameskipping (if possible) to recover the delay.
int throttle_wait(void);

// Return the approximate length of a frame, measured in hwcounter ticks 
// (see hwcounter.h).
uint32_t throttle_frame_length(void);

// Return the amount of time left before the end of the current frame.
// The number is expressed in hwcounter ticks (see hwcounter.h). If the
// number is negative, the current frame is using more than the time
// expected to match the requested pace.
int32_t throttle_frame_time_left(void);

#endif /* THROTTLE_H */
