#ifndef __LIBDRAGON_RSPQ_PROFILE_H
#define __LIBDRAGON_RSPQ_PROFILE_H

#include <stdint.h>
#include "rspq_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Profiling data of a single slot (for example an overlay) */
typedef struct rspq_profile_slot_s {
    uint64_t total_ticks;   ///< The total number of rcp ticks that were spent running in this slot
    uint64_t sample_count;  ///< The number of individual samples that were recorded
    const char *name;       ///< The name of this slot, if it is used; NULL otherwise
} rspq_profile_slot_t;

/** @brief RSPQ Profiling data */
typedef struct rspq_profile_data_s {
    rspq_profile_slot_t slots[RSPQ_PROFILE_SLOT_COUNT];     ///< The list of slots
    uint64_t total_ticks;                                   ///< The total elapsed rcp ticks since the last reset
    uint64_t rdp_busy_ticks;                                ///< The accumulated ticks sampled from DP_BUSY
    uint64_t frame_count;                                   ///< The number of recorded frames since the last reset
} rspq_profile_data_t;

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

/** @brief Copy the recorded data */
void rspq_profile_get_data(rspq_profile_data_t *data);

#ifdef __cplusplus
}
#endif

#endif
