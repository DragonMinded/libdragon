#include "rspq_profile.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "rsp.h"
#include "rspq.h"
#include "rspq_internal.h"

#if RSPQ_PROFILE

DEFINE_RSP_UCODE(rsp_profile);

#define CMD_PROFILE_FRAME 0x0

static rspq_profile_data_t profile_data;
static uint32_t ovl_id;

typedef struct {
    uint32_t cntr1;
    uint32_t padding1;

    rspq_profile_slot_dmem_t ovl[RSPQ_MAX_OVERLAYS];
    rspq_profile_slot_dmem_t cmn[RSPQ_PROFILE_CSLOT_COUNT];

    uint32_t frame_time;
    uint32_t busy_time;
    uint32_t frame_last;
    uint32_t busy_last;

    uint32_t padding2;
    uint32_t cntr2;

    uint32_t padding[2];
} profile_buffer_t;

_Static_assert(sizeof(profile_buffer_t) % 16 == 0, "profile_buffer_t size mismatch");

static volatile profile_buffer_t cur_profile_buffer __attribute__((aligned(16)));
static profile_buffer_t last_profile_buffer __attribute__((aligned(16)));

#define PROFILE_DATA_DMEM_ADDRESS (RSPQ_DATA_ADDRESS + offsetof(rsp_queue_t, rspq_profile_data))

void rspq_profile_reset(void)
{
    memset(&profile_data, 0, sizeof(profile_data));

    profile_data.slots[0].name = "Builtin cmds";
    for (int i = 1; i < RSPQ_MAX_OVERLAYS; i++)
    {
        if (!rspq_overlay_ucodes[i]) continue;
        if (i == ovl_id >> RSPQ_OVERLAY_ID_SHIFT) continue;
        if (i > 1 && rspq_overlay_ucodes[i] == rspq_overlay_ucodes[i-1]) continue;
        profile_data.slots[i].name = rspq_overlay_ucodes[i]->name;
    }

    profile_data.slots[RSPQ_PROFILE_CSLOT_WAIT_CPU].name = "Wait CPU";
    profile_data.slots[RSPQ_PROFILE_CSLOT_WAIT_RDP].name = "Wait RDP";
    profile_data.slots[RSPQ_PROFILE_CSLOT_WAIT_RDP_SYNCFULL].name = "Wait SYNC_FULL";
    profile_data.slots[RSPQ_PROFILE_CSLOT_WAIT_RDP_SYNCFULL_MULTI].name = "Wait SYNC_FULLx2";
    profile_data.slots[RSPQ_PROFILE_CSLOT_OVL_SWITCH].name = "Ovl Switch";
}

void rspq_profile_start()
{
    ovl_id = rspq_overlay_register(&rsp_profile);
    rspq_profile_reset();
}

void rspq_profile_stop()
{
    rspq_overlay_unregister(ovl_id);
}

static void rspq_profile_accumulate(void*)
{
    profile_buffer_t buf;
    do {
        buf = cur_profile_buffer;
    } while (buf.cntr1 != buf.cntr2);
    data_cache_hit_invalidate((void*)&cur_profile_buffer, sizeof(cur_profile_buffer));

    for (int i = 0; i < RSPQ_MAX_OVERLAYS; i++) {
        if (profile_data.slots[i].name == NULL) continue;
        profile_data.slots[i].total_ticks += buf.ovl[i].total_ticks - last_profile_buffer.ovl[i].total_ticks;
        profile_data.slots[i].sample_count += buf.ovl[i].sample_count - last_profile_buffer.ovl[i].sample_count;
    }

    int cbase = RSPQ_MAX_OVERLAYS;
    for (int i = 0; i < RSPQ_PROFILE_CSLOT_COUNT; i++) {
        profile_data.slots[cbase+i].total_ticks += buf.cmn[i].total_ticks - last_profile_buffer.cmn[i].total_ticks;
        profile_data.slots[cbase+i].sample_count += buf.cmn[i].sample_count - last_profile_buffer.cmn[i].sample_count;
    }

    profile_data.total_ticks += buf.frame_time - last_profile_buffer.frame_time;
    profile_data.rdp_busy_ticks += buf.busy_time - last_profile_buffer.busy_time;
    profile_data.frame_count++;

    last_profile_buffer = buf;
}

void rspq_profile_next_frame()
{
    rspq_write(ovl_id, CMD_PROFILE_FRAME, PhysicalAddr(&cur_profile_buffer));
    rspq_call_deferred(rspq_profile_accumulate, NULL);
}

#define RCP_TICKS_TO_USECS(ticks) (((ticks) * 1000000ULL) / RCP_FREQUENCY)
#define PERCENT(fraction, total) ((total) > 0 ? (float)(fraction) * 100.0f / (float)(total) : 0.0f)

static void rspq_profile_dump_overlay(rspq_profile_slot_t *slot, uint64_t frame_avg)
{
    uint64_t mean = slot->total_ticks / profile_data.frame_count;
    uint64_t mean_us = RCP_TICKS_TO_USECS(mean);
    float relative = PERCENT(mean, frame_avg);

    char buf[64];
    sprintf(buf, "%3.2f%%", relative);

    debugf("%-25s %10llu %10lluus %10s\n",
        slot->name,
        slot->sample_count / profile_data.frame_count,
        mean_us,
        buf);
}

void rspq_profile_dump()
{
    if (profile_data.frame_count == 0)
        return;

    uint64_t frame_avg = profile_data.total_ticks / profile_data.frame_count;
    uint64_t frame_avg_us = RCP_TICKS_TO_USECS(frame_avg);

    uint64_t counted_time = 0;
    for (size_t i = 0; i < RSPQ_PROFILE_SLOT_COUNT; i++) counted_time += profile_data.slots[i].total_ticks;
    
    // The counted time could be slightly larger than the total time due to various measurement errors
    uint64_t overhead_time = profile_data.total_ticks > counted_time ? profile_data.total_ticks - counted_time : 0;
    uint64_t overhead_avg = overhead_time / profile_data.frame_count;
    uint64_t overhead_us = RCP_TICKS_TO_USECS(overhead_avg);

    float overhead_relative = PERCENT(overhead_avg, frame_avg);

    uint64_t rdp_busy_avg = profile_data.rdp_busy_ticks / profile_data.frame_count;
    uint64_t rdp_busy_us = RCP_TICKS_TO_USECS(rdp_busy_avg);
    float rdp_utilisation = PERCENT(rdp_busy_avg, frame_avg);

    debugf("%-25s %10s %12s %10s\n", "Slot", "Cnt/Frame", "Avg/Frame", "Rel/Frame");
	debugf("------------------------------------------------------------\n");

    for (int i = 0; i < RSPQ_PROFILE_SLOT_COUNT; i++) {
        if (!profile_data.slots[i].name) continue;
        rspq_profile_dump_overlay(&profile_data.slots[i], frame_avg);
    }

	debugf("------------------------------------------------------------\n");
    debugf("Profiled frames:    %12lld\n", profile_data.frame_count);
    debugf("Frames per second:  %12.1f\n", (float)RCP_FREQUENCY/(float)frame_avg);
    debugf("Average frame time: %10lldus\n", frame_avg_us);
    debugf("RDP busy time:      %10lldus (%2.2f%%)\n", rdp_busy_us, rdp_utilisation);
    debugf("Unrecorded time:    %10lldus (%2.2f%%)\n", overhead_us, overhead_relative);
    debugf("\n");
}

void rspq_profile_get_data(rspq_profile_data_t *data)
{
    memcpy(data, &profile_data, sizeof(profile_data));

}

#else
void rspq_profile_start(void) { }
void rspq_profile_stop(void) { }
void rspq_profile_reset(void) { }
void rspq_profile_next_frame(void) { }
void rspq_profile_dump(void) { }
void rspq_profile_get_data(rspq_profile_data_t *data) { }
#endif