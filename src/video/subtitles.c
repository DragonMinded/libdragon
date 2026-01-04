/**
 * @file subtitles.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Subtitles API via the SUB64 format
 * @ingroup video
 *
 * @section sub64_format SUB64 subtitle stream format (v1)
 *
 * This module consumes subtitle streams produced by `videoconv64` as `.sub64` sidecars.
 *
 * The `.sub64` file is typically stored **asset-compressed** (eg. `DCA3` header). In that case,
 * all offsets described below are relative to the **decompressed SUB64 payload**, not the compressed
 * bytes on disk.
 *
 * @subsection sub64_layout Layout and endianness
 *
 * - Multi-byte integers and floats are **big-endian**.
 * - `DELT` uses **VarUInt LEB128** (7-bit groups, little-endian base-128).
 * - Layout is fixed and sequential: `[Header][IDX0][DELT][OPC0][TXT0]`.
 *
 * @subsection sub64_header Header (fixed 36 bytes)
 *
 * - `char magic[5]` = `"SUB64"`
 * - `u8 version` = `1`
 * - `u16 flags` (currently 0)
 * - `f32 fps` (IEEE754, big-endian)
 * - `u32 num_frames` (number of frames in the subtitle stream)
 * - `u32 num_syncs` (number of syncpoints)
 * - `u32 idx_off` (absolute byte offset to the IDX0 section)
 * - `u32 delt_off` (absolute byte offset to the DELT section)
 * - `u32 opc_off` (absolute byte offset to the OPC0 section)
 * - `u32 txt_off` (absolute byte offset to the TXT0 section)
 *
 * @subsection sub64_idx0 IDX0 (seek index)
 *
 * `IDX0` is an array of `idx0_count` fixed-size entries (16 bytes each):
 *
 * - `u32 sync_frame`
 * - `u32 delt_off` absolute byte offset to a position inside `DELT`
 * - `u32 opc_off`  absolute byte offset to a position inside `OPC0`
 * - `u32 txt_off`  absolute byte offset to a position inside `TXT0`
 *
 * `videoconv64` chooses syncpoints in **gaps** (moments with no active subtitles) in a best-effort way
 * to split the stream into roughly equal chunks. `sync_frame = 0` is always present and duplicates
 * (eg. due to long gaps) are omitted.
 *
 * @subsection sub64_streams Streams
 *
 * Event i is read in lock-step:
 *
 * - `delta_frames = read_varuint(DELT)`
 * - `opcode = OPC0[i]`
 *
 * Some opcodes also consume one NUL-terminated UTF-8 string from `TXT0` (in order).
 *
 * @subsection sub64_opcodes Opcodes and regions
 *
 * At most **one cue per region** can be visible at a time (3 regions: bottom/top/center).
 * If multiple input cues overlap in the same region, the converter keeps the **latest** one:
 * a newer `SHOW_*` replaces the previous cue in that region. When an older cue ends, it does
 * not produce a `HIDE_*` if it was already replaced.
 *
 * SHOW opcodes select one of 3 fixed screen regions (minimal REGION support):
 *
 * - `0x01` SHOW_BOTTOM: show next string in the bottom region
 * - `0x02` SHOW_TOP:    show next string in the top region
 * - `0x03` SHOW_CENTER: show next string in the center region
 *
 * HIDE opcodes hide the current cue in a specific region:
 *
 * - `0x10` HIDE_BOTTOM
 * - `0x11` HIDE_TOP
 * - `0x12` HIDE_CENTER
 *
 * Reserved:
 *
 * - `0x04` CLEAR: hide all subtitles (not currently emitted by the tool)
 *
 * @subsection sub64_text Text and markup
 *
 * `TXT0` contains UTF-8 strings terminated by `\\0`.
 *
 * `videoconv64` converts a minimal WebVTT subset to `rdpq_text` escapes:
 *
 * - `<i> .. </i>` -> `$02 ... $01`
 * - `<b> .. </b>` -> `$03 ... $01`
 * - `<u>` is mapped to `$04` in the converter (best-effort)
 * - `<br>` -> newline
 *
 */

#include "subtitles.h"
#include "../asset.h"
#include "../utils.h"
#include "rdpq_text.h"
#include "rdpq_font.h"
#include "debug.h"
#include <stddef.h>
#include <stdlib.h>

typedef enum {
	SUB64_REGION_BOTTOM = 0,
	SUB64_REGION_TOP    = 1,
	SUB64_REGION_CENTER = 2,
} sub64_region_t;

typedef enum {
    // SHOW opcodes select one of 3 fixed regions on N64: bottom/top/center.
    SUB64_OP_SHOW        = 0x00,
    SUB64_OP_SHOW_BOTTOM = SUB64_OP_SHOW + SUB64_REGION_BOTTOM,
    SUB64_OP_SHOW_TOP    = SUB64_OP_SHOW + SUB64_REGION_TOP,
    SUB64_OP_SHOW_CENTER = SUB64_OP_SHOW + SUB64_REGION_CENTER,
    // HIDE opcodes hide the current cue in a specific region.
    SUB64_OP_HIDE        = 0x10,
    SUB64_OP_HIDE_BOTTOM = SUB64_OP_HIDE + SUB64_REGION_BOTTOM,
    SUB64_OP_HIDE_TOP    = SUB64_OP_HIDE + SUB64_REGION_TOP,
    SUB64_OP_HIDE_CENTER = SUB64_OP_HIDE + SUB64_REGION_CENTER,
    SUB64_OP_CLEAR = 0x04,
} sub64_opcode_t;

typedef struct seek_entry_s {
    uint32_t sync_frame;
    uint8_t *deltas;
    uint8_t *opcodes;
    char *txts;
} seek_entry_t;

typedef struct sub_state_s {
    subtitle_cue_t cues[3];     ///< Bottom, top, center
    int begin_frame_idx;        ///< Frame index at which this state begins
    int end_frame_idx;          ///< Frame index at which next state begins (exclusive bound)
    const uint8_t *cur_delta;   ///< Current position in DELT stream
    const uint8_t *cur_opcode;  ///< Current position in OPC0 stream
    char *cur_txt;              ///< Current position in TXT0 stream
} sub_state_t;

typedef struct subtitles_s {
    char magic[5];
    uint8_t version;
    uint16_t flags;
    float fps;
    uint32_t num_frames;
    uint32_t num_syncs;
    struct {                        ///< Runtime state (empty in the file)
        sub_state_t state;            ///< Current subtitle state
        sub_state_t next_state;       ///< Subtitle state at next change
        int cur_frame_idx;            ///< Current frame index
    };
    seek_entry_t seek_entries[];
} subtitles_t;

_Static_assert(offsetof(subtitles_t, seek_entries) - offsetof(subtitles_t, state) == 44+44+4,
               "invalid runtime state layout");

#define PTR_DECODE(sub, ptr)    ((void*)(((uint8_t*)(sub)) + (uint32_t)(ptr)))

subtitles_t* subtitles_load(const char *fn) {
    subtitles_t *sub = asset_load(fn, NULL);
    assertf(memcmp(sub->magic, "SUB64", 5) == 0, "Invalid subtitle file: %s", fn);
    assertf(sub->version == 1, "Unsupported subtitle version %d in file: %s", sub->version, fn);

    for (int i=0; i<sub->num_syncs; i++) {
        seek_entry_t *entry = &sub->seek_entries[i];
        entry->deltas  = PTR_DECODE(sub, entry->deltas);
        entry->opcodes = PTR_DECODE(sub, entry->opcodes);
        entry->txts    = PTR_DECODE(sub, entry->txts);
    }

    subtitles_seek(sub, 0);
    return sub;
}

void subtitles_free(subtitles_t *sub) {
    free(sub);
}

static void parse_next(sub_state_t *state)
{
    do {
        state->begin_frame_idx += __read_varint_u64(&state->cur_delta);
        sub64_opcode_t opcode = *state->cur_opcode++;

        switch (opcode) {
            case SUB64_OP_SHOW_BOTTOM:
            case SUB64_OP_SHOW_TOP:
            case SUB64_OP_SHOW_CENTER: {
                sub64_region_t region = (sub64_region_t)(opcode - SUB64_OP_SHOW);
                state->cues[region].text = state->cur_txt;
                // Advance cur_txt to next NUL
                while (*state->cur_txt++ != '\0') {}
                state->cues[region].region_hint = (int8_t)region;
                state->cues[region].visible = true;
                break;
            }
            case SUB64_OP_HIDE_BOTTOM:
            case SUB64_OP_HIDE_TOP:
            case SUB64_OP_HIDE_CENTER: {
                sub64_region_t region = (sub64_region_t)(opcode - SUB64_OP_HIDE);
                state->cues[region].visible = false;
                break;
            }
            case SUB64_OP_CLEAR: {
                for (int i=0; i<3; i++) {
                    state->cues[i].visible = false;
                }
                break;
            }
            default:
                assertf(0, "Unknown subtitle opcode: 0x%02X", opcode);
        }
    } while (__peek_varint_u64(state->cur_delta) == 0);

    state->end_frame_idx = state->begin_frame_idx + __peek_varint_u64(state->cur_delta);
}

void subtitles_next_frame(subtitles_t *sub)
{
    sub->cur_frame_idx++;
    if (sub->cur_frame_idx >= sub->state.end_frame_idx) {
        parse_next(&sub->state);
        assertf(sub->cur_frame_idx >= sub->state.begin_frame_idx && sub->cur_frame_idx < sub->state.end_frame_idx,
                "Subtitle state frame indices invalid after parsing: cur=%d, begin=%d, end=%d",
                sub->cur_frame_idx, sub->state.begin_frame_idx, sub->state.end_frame_idx);

        debugf("Subtitles advanced to frame %d: begin_frame_idx=%d, end_frame_idx=%d\n",
               sub->cur_frame_idx, sub->state.begin_frame_idx, sub->state.end_frame_idx);
        for (int i=0; i<3; i++) {
            if (sub->state.cues[i].visible)
            debugf("    Region %d: visible=%d, text=\"%s\"\n",
                   i, sub->state.cues[i].visible, sub->state.cues[i].text ? sub->state.cues[i].text : "");
        }
    }
}

int subtitles_get_current_cues(subtitles_t *sub, subtitle_cue_t *cues, int max_cues)
{
    int count = 0;
    for (int i=0; i<3 && count < max_cues; i++) {
        if (sub->state.cues[i].visible) {
            cues[count++] = sub->state.cues[i];
        }
    }
    return count;
}

void subtitles_seek(subtitles_t *sub, int frame_idx)
{
    frame_idx = CLAMP(frame_idx, 0, (int)sub->num_frames - 1);

    for (int i=0; i<sub->num_syncs; i++)
        debugf("  Seek entry %d: sync_frame=%d\n", i, sub->seek_entries[i].sync_frame);

    // Find the closest seek entry via binary search
    int lo = 0, hi = sub->num_syncs;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if ((int)sub->seek_entries[mid].sync_frame <= frame_idx)
            lo = mid + 1;
        else
            hi = mid;
    }
    int seek_idx = lo - 1;
    if (seek_idx < 0) seek_idx = 0;

    // Initialize state from the seek entry. All seek entries are
    // guaranteed to have zero active cues, so that the state is well-defined.
    seek_entry_t *entry = &sub->seek_entries[seek_idx];
    sub->state = (sub_state_t){
        .cur_delta = entry->deltas,
        .cur_opcode = entry->opcodes,
        .cur_txt = entry->txts,
        .begin_frame_idx = (int)entry->sync_frame,
        .end_frame_idx = (int)entry->sync_frame + __peek_varint_u64(entry->deltas),
    };

    debugf("Subtitles seek to frame %d using seek entry %d (sync_frame=%d)\n",
           frame_idx, seek_idx, entry->sync_frame);
    debugf("  Initial state: begin_frame_idx=%d, end_frame_idx=%d\n",
           sub->state.begin_frame_idx, sub->state.end_frame_idx);

    // Parse states until we reach the desired frame. We stop once we
    // reach a state whose range contains frame_idx.
    while (sub->state.end_frame_idx <= frame_idx) {
        parse_next(&sub->state);
    }

    // Set current frame index
    sub->cur_frame_idx = frame_idx;

    debugf("  Final state: begin_frame_idx=%d, end_frame_idx=%d, cur_frame_idx=%d\n",
           sub->state.begin_frame_idx, sub->state.end_frame_idx, sub->cur_frame_idx);
}

typedef struct subrenderer_s subrenderer_t;

typedef struct subrenderer_s {
    void (*render)(subrenderer_t *renderer, subtitle_cue_t *cues, int num_cues);
    void (*free)(subrenderer_t *renderer);
} subrenderer_t;


typedef struct {
    subrenderer_t base;
    int canvas_width;
    int canvas_height;
    subrenderer_rdpq_parms_t parms;
    rdpq_font_t *fonts[3]; // 0=normal, 1=bold, 2=italic
} subrenderer_rdpq_t;


static void subrenderer_rdpq_render(subrenderer_t* base, subtitle_cue_t *cues, int num_cues)
{
    subrenderer_rdpq_t *sr = (subrenderer_rdpq_t*)base;

    const int WIDTH_MARGIN = 10;

    int x0 = WIDTH_MARGIN;
    int y0[3] = {
        sr->canvas_height - 60,               // bottom
        10,                                   // top
        sr->canvas_height / 2 - 40            // center
    };
    rdpq_valign_t valigns[3] = {
        VALIGN_BOTTOM,
        VALIGN_TOP,
        VALIGN_CENTER
    };

    rdpq_textparms_t textparms = {
        .width = sr->canvas_width - 2 * WIDTH_MARGIN,
        .height = sr->canvas_height / 4,
        .align = ALIGN_CENTER,
        .wrap = WRAP_WORD,
        .disable_aa_fix = true,
    };

    for (int i=0; i<num_cues; i++) {
        subtitle_cue_t *cue = &cues[i];
        if (!cue->visible) continue;
        textparms.valign = valigns[cue->region_hint];
        rdpq_text_print(&textparms, i+1, x0, y0[cue->region_hint], cue->text);
    }
}

static void subrenderer_rdpq_free(subrenderer_t* base) {
    subrenderer_rdpq_t *renderer = (subrenderer_rdpq_t*)base;
    for (int i = 0; i < 3; i++) {
        if (renderer->fonts[i]) {
            rdpq_text_unregister_font(i+1);
            rdpq_font_free(renderer->fonts[i]);
        }
    }
}

subrenderer_t* subrenderer_rdpq_create(int width, int height, subrenderer_rdpq_parms_t *parms)
{
    subrenderer_rdpq_t *renderer = malloc(sizeof(subrenderer_rdpq_t));
    assertf(renderer, "Out of memory");
    memset(renderer, 0, sizeof(subrenderer_rdpq_t));
    if (parms) renderer->parms = *parms;

    renderer->base.render = subrenderer_rdpq_render;
    renderer->base.free = subrenderer_rdpq_free;
    renderer->canvas_width = width;
    renderer->canvas_height = height;

    for (int i = 0; i < 3; i++) {
        if (renderer->parms.fonts[i]) {
            renderer->fonts[i] = rdpq_font_load(renderer->parms.fonts[i]);
        } else if (i == 0) {
            // Load default font for normal text if not specified
            renderer->fonts[i] = rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_VAR);
        }

        // Register the font (or fallback to normal)
        if (!renderer->fonts[i]) {
            rdpq_text_register_font(i+1, renderer->fonts[i]);
        } else {
            rdpq_text_register_font(i+1, renderer->fonts[0]);
        }
    }

    return &renderer->base;
}

void subrenderer_render(subrenderer_t *renderer, subtitle_cue_t *cues, int num_cues) {
    renderer->render(renderer, cues, num_cues);
}

void subrenderer_free(subrenderer_t *renderer) {
    renderer->free(renderer);
    free(renderer);
}   

