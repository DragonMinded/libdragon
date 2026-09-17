/**
* @copyright 2024 - Max Bebök (modified by snacchus)
* @license MIT
*/
#ifndef DEBUG_OVERLAY_H
#define DEBUG_OVERLAY_H

#include <libdragon.h>
#include <rspq_profile.h>

#define DEBUG_OVERLAY_FONT_ID           UINT8_MAX
#define DEBUG_OVERLAY_DEFAULT_STYLE_ID  0
#define DEBUG_OVERLAY_ACCENT_STYLE_ID   1
#define DEBUG_OVERLAY_MUTED_STYLE_ID    2
#define DEBUG_OVERLAY_DARK_STYLE_ID     3
#define DEBUG_OVERLAY_TEXT_YOFFSET      10

typedef struct {
  uint32_t calls;
  int32_t timeUs;
  uint32_t index;
  color_t color;
  bool isIdle;
  const char *name;
} ProfileSlot;

const static color_t THEME_COLORS[10] = {
  (color_t){ 0xd4, 0x3d, 0x51, 0xff},
  (color_t){ 0xea, 0x7e, 0x54, 0xff},
  (color_t){ 0xf7, 0xb8, 0x6f, 0xff},
  (color_t){ 0xff, 0xee, 0xa1, 0xff},
  (color_t){ 0xb4, 0xce, 0x85, 0xff},
  (color_t){ 0x6a, 0xab, 0x75, 0xff},
  (color_t){ 0x00, 0x87, 0x6c, 0xff},
  (color_t){ 0x00, 0x5a, 0x5a, 0xff},
  (color_t){ 0x00, 0x3e, 0x5c, 0xff},
  (color_t){ 0x00, 0x2c, 0x4e, 0xff}
};

static void debug_draw_line_hori(float x, float y, float width) {
  rdpq_fill_rectangle(x, y, x + width, y + 1);
}

static void debug_draw_line_vert(float x, float y, float height) {
  rdpq_fill_rectangle(x, y, x + 1, y + height);
}

static void debug_draw_color_rect(float x, float y, float width, float height, color_t color) {
  rdpq_set_fill_color(color);
  rdpq_fill_rectangle(x, y, x + width, y + height);
}

static void draw_circle_slice(float x, float y, float radius, float startAngle, float endAngle, color_t color)
{
    rdpq_set_prim_color(color);

    float angleStep = 0.05f;
    float x1 = x + fm_cosf(startAngle) * radius;
    float y1 = y + fm_sinf(startAngle) * radius;
    for(float angle = startAngle; angle < endAngle; angle += angleStep) {
      float angleNext = angle + angleStep;
      float x2 = x + fm_cosf(angleNext) * radius;
      float y2 = y + fm_sinf(angleNext) * radius;
      rdpq_triangle(&TRIFMT_FILL, (float[]){ x, y }, (float[]){ x1, y1 }, (float[]){ x2, y2 });
      x1 = x2;
      y1 = y2;
    }
}

static void debug_profile_patch_name(ProfileSlot *slot)
{
  switch (slot->index) {
    case 0: slot->name = "builtins"; break;
    case RSPQ_PROFILE_CSLOT_WAIT_CPU: slot->name = "CPU"; break;
    case RSPQ_PROFILE_CSLOT_WAIT_RDP: slot->name = "RDP"; break;
    case RSPQ_PROFILE_CSLOT_WAIT_RDP_SYNCFULL: slot->name = "SYNC_FULL"; break;
    case RSPQ_PROFILE_CSLOT_WAIT_RDP_SYNCFULL_MULTI: slot->name = "multi SYNC_F"; break;
    case RSPQ_PROFILE_CSLOT_OVL_SWITCH: slot->name = "Ovl switch"; break;
  }
}

static int debug_profile_slot_compare(const void * a, const void * b) {
  const ProfileSlot *slotA = (const ProfileSlot *)a;
  const ProfileSlot *slotB = (const ProfileSlot *)b;
  int timeDiff = slotB->timeUs - slotA->timeUs;
  return timeDiff == 0 ? (slotB->calls - slotA->calls) : timeDiff;
}

static bool debug_profile_is_idle(uint32_t index) {
  return index == RSPQ_PROFILE_CSLOT_WAIT_CPU || index == RSPQ_PROFILE_CSLOT_WAIT_RDP
    || index == RSPQ_PROFILE_CSLOT_WAIT_RDP_SYNCFULL || index == RSPQ_PROFILE_CSLOT_WAIT_RDP_SYNCFULL_MULTI;
}

static void debug_print_table_entry(ProfileSlot *slot, float posX, float *posY)
{
  if(slot->calls != 0) {
  rdpq_text_printf(NULL, DEBUG_OVERLAY_FONT_ID, posX, *posY+DEBUG_OVERLAY_TEXT_YOFFSET, "%-10.10s %5lu %7luu", slot->name, slot->calls, slot->timeUs);
  } else {
    rdpq_text_printf(NULL, DEBUG_OVERLAY_FONT_ID, posX, *posY+DEBUG_OVERLAY_TEXT_YOFFSET, "%-10.10s     - %7luu", slot->name, slot->timeUs);
  }
  *posY += 10;
}


#define RCP_TICKS_TO_USECS(ticks) (((ticks) * 1000000ULL) / RCP_FREQUENCY)

static rspq_profile_data_t profile_data;

void debug_overlay_init()
{
    rdpq_font_t *font = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO);
    rdpq_text_register_font(DEBUG_OVERLAY_FONT_ID, font);
    rdpq_font_style(font, DEBUG_OVERLAY_ACCENT_STYLE_ID, &(rdpq_fontstyle_t) {
      .color = RGBA32(0x99, 0x99, 0xEE, 0xFF)
    });
    rdpq_font_style(font, DEBUG_OVERLAY_MUTED_STYLE_ID, &(rdpq_fontstyle_t) {
      .color = RGBA32(0xAA, 0xAA, 0xAA, 0xFF)
    });
    rdpq_font_style(font, DEBUG_OVERLAY_DARK_STYLE_ID, &(rdpq_fontstyle_t) {
      .color = RGBA32(0x55, 0x55, 0x55, 0x99)
    });
}

void debug_draw_perf_overlay(float measuredFps)
{
    if(profile_data.frame_count == 0)return;

    const float TABLE_POS_X = 104;
    const float TABLE_POS_Y = 12;
    const float FRAME_BARS_POS_Y = 178;

    const float PIE_RADIUS = 33;
    const float PIE_POS_BUSY[2] = {48, 48};
    const float PIE_POS_WAIT[2] = {48, PIE_POS_BUSY[1] + PIE_RADIUS*2 + 7};

    uint64_t totalTicks = 0;
    uint32_t timeTotalBusy = 0;
    uint32_t timeTotalWait = 0;

    float posY = TABLE_POS_Y;
    rdpq_text_print(NULL, DEBUG_OVERLAY_FONT_ID, TABLE_POS_X, posY+DEBUG_OVERLAY_TEXT_YOFFSET, "Tasks      Calls     Time");
    posY += 12;

    // Copy & convert performance data to a sortable array
    const uint32_t SLOT_COUNT = RSPQ_PROFILE_SLOT_COUNT + 1;
    ProfileSlot slots[SLOT_COUNT];
    for(size_t i = 0; i < RSPQ_PROFILE_SLOT_COUNT; i++)
    {
      ProfileSlot *slot = &slots[i];
      slot->index = i;
      slot->isIdle = debug_profile_is_idle(i);
      slot->name = profile_data.slots[i].name;

      if(slot->name == NULL)continue;
      debug_profile_patch_name(slot);

      totalTicks += profile_data.slots[i].total_ticks;

      slot->calls = (uint32_t)(profile_data.slots[i].sample_count / profile_data.frame_count);
      slot->timeUs = RCP_TICKS_TO_USECS(profile_data.slots[i].total_ticks / profile_data.frame_count);

      if(slot->name[0] == 'r' && slot->name[1] == 's' && slot->name[2] == 'p') {
        slot->name += 4;
      }

      if(slot->isIdle) {
        timeTotalWait += slot->timeUs;
      } else {
        timeTotalBusy += slot->timeUs;
      }
    }

    // Calc. global times
    uint64_t dispatchTicks = profile_data.total_ticks > totalTicks ? profile_data.total_ticks - totalTicks : 0;
    uint64_t dispatchTime = RCP_TICKS_TO_USECS(dispatchTicks / profile_data.frame_count);
    uint64_t rdpTimeBusy = RCP_TICKS_TO_USECS(profile_data.rdp_busy_ticks / profile_data.frame_count);

    // Add dispatch time into the slots
    slots[SLOT_COUNT - 1] = (ProfileSlot){
      .index = SLOT_COUNT - 1,
      .name = "Cmd process",
      .calls = 0,
      .timeUs = dispatchTime
    };
    timeTotalBusy += dispatchTime;

    // Now sort for both the table and pie chart
    qsort(slots, SLOT_COUNT, sizeof(ProfileSlot), debug_profile_slot_compare);

    // Draw table (texts, busy)
    uint32_t colorIndex = 0;
    for(size_t i = 0; i < SLOT_COUNT; i++)
    {
      if(slots[i].isIdle || slots[i].name == NULL)continue;
      slots[i].color = THEME_COLORS[colorIndex % 8];
      colorIndex++;
      debug_print_table_entry(&slots[i], TABLE_POS_X, &posY);
    }

    posY += 2; float endSectionOvlY = posY;
    posY += 1;

    // Table - Total Waits
    rdpq_text_printf(&(rdpq_textparms_t){.style_id = DEBUG_OVERLAY_ACCENT_STYLE_ID}, DEBUG_OVERLAY_FONT_ID, TABLE_POS_X, posY+DEBUG_OVERLAY_TEXT_YOFFSET, "Total (busy)     %7ldu", (uint32_t)timeTotalBusy);

    posY += 12; float endSectionTotalBusyY = posY;
    posY += 6;

    colorIndex = 4;
    for(size_t i = 0; i < SLOT_COUNT; i++)
    {
      if(!slots[i].isIdle || slots[i].name == NULL)continue;
      slots[i].color = THEME_COLORS[colorIndex % 8];
      colorIndex++;
      debug_print_table_entry(&slots[i], TABLE_POS_X, &posY);
    }

    posY += 2; float endSectionCPUY = posY;
    posY += 1;

    // Table - Total Waits
    rdpq_text_printf(&(rdpq_textparms_t){.style_id = DEBUG_OVERLAY_ACCENT_STYLE_ID}, DEBUG_OVERLAY_FONT_ID, TABLE_POS_X, posY+DEBUG_OVERLAY_TEXT_YOFFSET, "Total (waiting)  %7ldu", (uint32_t)timeTotalWait);

    posY += 12; float endSectionTotalWaitsY = posY;
    posY += 10;

    // Pie Chart
    float angleOffset = -1.57079632679f;


    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER_FLAT);

    if(profile_data.frame_count != 1)
    {
    for(int type=0; type<2; ++type)
    {
      for(size_t i = 0; i < RSPQ_PROFILE_SLOT_COUNT; i++)
      {
        ProfileSlot *slot = &slots[i];
        if(slot->isIdle != type || slot->name == NULL)continue;
        const float* pos = type == 0 ? PIE_POS_BUSY : PIE_POS_WAIT;
        float refTotal = type == 0 ? timeTotalBusy : timeTotalWait;

        float slotAngle = 0;
        if(refTotal > 0)slotAngle = (float)slot->timeUs / (float)refTotal * 6.28318530718f;

        if(slotAngle > 0.01f) {
          draw_circle_slice(pos[0], pos[1], PIE_RADIUS, angleOffset, angleOffset + slotAngle, slot->color);
        }
        angleOffset += slotAngle;
      }
    }
    }

    // RCP performance bars
    float timeScale = 1.0f / (200.0f); // 1 / (us per pixel)
    posY = FRAME_BARS_POS_Y;
    float barPos[2] = {48, posY+16};
    const float BAR_HEIGHT = 10;
    const float BAR_BORDER = 2;

    float busyWidth = (float)timeTotalBusy * timeScale;
    float idleWidth = (float)timeTotalWait * timeScale;
    float rdpBusyWidth = (float)rdpTimeBusy * timeScale;

    float posFps60 = (1000000.0f / 60.0f) * timeScale;
    float posFps30 = (1000000.0f / 30.0f) * timeScale;
    float posFps20 = (1000000.0f / 20.0f) * timeScale;

    // bar (idle vs busy) - Text
    rdpq_text_print(NULL, DEBUG_OVERLAY_FONT_ID, barPos[0]-30, posY+DEBUG_OVERLAY_TEXT_YOFFSET + 4 + (BAR_HEIGHT+BAR_BORDER), "RSP");
    rdpq_text_print(NULL, DEBUG_OVERLAY_FONT_ID, barPos[0]-30, posY+DEBUG_OVERLAY_TEXT_YOFFSET + 4 + (BAR_HEIGHT+BAR_BORDER)*2, "RDP");

    float fpsMarkerY = posY + 8 + (BAR_HEIGHT+BAR_BORDER)*3;
    rdpq_text_print(&(rdpq_textparms_t){.style_id = DEBUG_OVERLAY_MUTED_STYLE_ID}, DEBUG_OVERLAY_FONT_ID, barPos[0]-30, fpsMarkerY+DEBUG_OVERLAY_TEXT_YOFFSET, "FPS Target:");

    // FPS marker at bottom of lines
    rdpq_text_printf(&(rdpq_textparms_t){.style_id = DEBUG_OVERLAY_MUTED_STYLE_ID}, DEBUG_OVERLAY_FONT_ID, barPos[0] + floorf(posFps60) - 14, fpsMarkerY+DEBUG_OVERLAY_TEXT_YOFFSET, "60");
    rdpq_text_printf(&(rdpq_textparms_t){.style_id = DEBUG_OVERLAY_MUTED_STYLE_ID}, DEBUG_OVERLAY_FONT_ID, barPos[0] + floorf(posFps30) - 14, fpsMarkerY+DEBUG_OVERLAY_TEXT_YOFFSET, "30");
    rdpq_text_printf(&(rdpq_textparms_t){.style_id = DEBUG_OVERLAY_MUTED_STYLE_ID}, DEBUG_OVERLAY_FONT_ID, barPos[0] + floorf(posFps20) - 14, fpsMarkerY+DEBUG_OVERLAY_TEXT_YOFFSET, "20");

    rdpq_text_printf(&(rdpq_textparms_t){.style_id = DEBUG_OVERLAY_ACCENT_STYLE_ID}, DEBUG_OVERLAY_FONT_ID, barPos[0] + 120, posY+DEBUG_OVERLAY_TEXT_YOFFSET, "FPS: %.2f", measuredFps);
    rdpq_text_printf(&(rdpq_textparms_t){.style_id = DEBUG_OVERLAY_MUTED_STYLE_ID}, DEBUG_OVERLAY_FONT_ID, barPos[0] + 208, posY+DEBUG_OVERLAY_TEXT_YOFFSET, "(f:%lld)", profile_data.frame_count);

    // ======== FILL MODE ========

    rdpq_set_mode_fill(RGBA32(0x22, 0x22, 0x22, 0xFF));

    // table lines
    const float LINE_POS_X = 94;
    const float LINE_SIZE_X = 210;
    rdpq_set_fill_color(RGBA32(44, 44, 44, 0xFF));
    debug_draw_line_hori(LINE_POS_X,    TABLE_POS_Y+11,     LINE_SIZE_X);
    debug_draw_line_hori(LINE_POS_X,    endSectionOvlY,     LINE_SIZE_X);
    debug_draw_line_hori(LINE_POS_X+10, endSectionCPUY,     LINE_SIZE_X-10);

    debug_draw_line_hori(LINE_POS_X+10, endSectionTotalBusyY,   LINE_SIZE_X-10);
    debug_draw_line_hori(LINE_POS_X+10, endSectionTotalBusyY+2, LINE_SIZE_X-10);

    debug_draw_line_hori(LINE_POS_X+10, endSectionTotalWaitsY,   LINE_SIZE_X-10);
    debug_draw_line_hori(LINE_POS_X+10, endSectionTotalWaitsY+2, LINE_SIZE_X-10);

    // background till end of frame range
    rdpq_fill_rectangle(barPos[0]-2, barPos[1]-2, barPos[0]+posFps20, barPos[1] + (BAR_HEIGHT + BAR_BORDER)*2);

    // RSP busy + idle bar
    debug_draw_color_rect(barPos[0],             barPos[1], busyWidth, BAR_HEIGHT, RGBA32(0x44, 0x44, 0xAA, 0xFF));
    debug_draw_color_rect(barPos[0] + busyWidth, barPos[1], idleWidth, BAR_HEIGHT, RGBA32(0xAA, 0xAA, 0xAA, 0xFF));

    // RDP busy (purple)
    debug_draw_color_rect(barPos[0], barPos[1] + BAR_HEIGHT + BAR_BORDER, rdpBusyWidth, BAR_HEIGHT, RGBA32(0xAA, 0x44, 0xAA, 0xFF));

    // lines marking frame-rates
    rdpq_set_fill_color(RGBA32(0xFF, 0xFF, 0xFF, 0xFF));
    debug_draw_line_vert(barPos[0] + posFps60, barPos[1]-BAR_BORDER, BAR_HEIGHT + 30);
    debug_draw_line_vert(barPos[0] + posFps30, barPos[1]-BAR_BORDER, BAR_HEIGHT + 30);
    debug_draw_line_vert(barPos[0] + posFps20, barPos[1]-BAR_BORDER, BAR_HEIGHT + 30);

    // total time on right side of bar
    if((busyWidth+idleWidth) < 150) {
      rdpq_text_printf(&(rdpq_textparms_t){.style_id = DEBUG_OVERLAY_DARK_STYLE_ID}, DEBUG_OVERLAY_FONT_ID, barPos[0]+198, posY+DEBUG_OVERLAY_TEXT_YOFFSET + 3 + (BAR_HEIGHT+BAR_BORDER), "%7luu", timeTotalBusy + timeTotalWait);
      rdpq_text_printf(&(rdpq_textparms_t){.style_id = DEBUG_OVERLAY_DARK_STYLE_ID}, DEBUG_OVERLAY_FONT_ID, barPos[0]+198, posY+DEBUG_OVERLAY_TEXT_YOFFSET + 3 + (BAR_HEIGHT+BAR_BORDER)*2, "%7lluu", rdpTimeBusy);
    } else {
      rdpq_text_printf(&(rdpq_textparms_t){.style_id = DEBUG_OVERLAY_DARK_STYLE_ID}, DEBUG_OVERLAY_FONT_ID, barPos[0]+2, posY+DEBUG_OVERLAY_TEXT_YOFFSET + 3 + (BAR_HEIGHT+BAR_BORDER), "%luu", timeTotalBusy + timeTotalWait);
      rdpq_text_printf(&(rdpq_textparms_t){.style_id = DEBUG_OVERLAY_DARK_STYLE_ID}, DEBUG_OVERLAY_FONT_ID, barPos[0]+2, posY+DEBUG_OVERLAY_TEXT_YOFFSET + 3 + (BAR_HEIGHT+BAR_BORDER)*2, "%lluu", rdpTimeBusy);
    }
}

#endif //DEBUG_OVERLAY_H