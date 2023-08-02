#ifndef __LIBDRAGON_RSPQ_PROFILE_H
#define __LIBDRAGON_RSPQ_PROFILE_H

/** @brief Start the rspq profiler */
void rspq_profile_start(void);

/** @brief Stop the rspq profiler */
void rspq_profile_stop(void);

/** @brief Reset the rspq profiler and discard any recorded samples */
void rspq_profile_reset(void);

/** @brief Mark the start of the next frame to the rspq profiler */
void rspq_profile_next_frame(void);

/** @brief Dump the recorded data to the console */
void rspq_profile_dump(void);

#endif
