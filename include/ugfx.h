/**
 * @file ugfx.h
 * @brief Microcode Graphics
 * @ingroup ugfx
 */

#ifndef __LIBDRAGON_UGFX_H
#define __LIBDRAGON_UGFX_H

#include <stdbool.h>
#include <stdint.h>
#include <display.h>

#define UGFX_MIN_RDP_BUFFER_SIZE 0x2B0
#define UGFX_DEFAULT_RDP_BUFFER_SIZE (UGFX_MIN_RDP_BUFFER_SIZE * 4)

#define Z_MAX 0x03FE

#define PACK_RGBA16(r, g, b, a) (((r & 0xF8) << 8) | ((g & 0xF8) << 3) | ((b & 0xF8) >> 2) | (a >> 7))
#define PACK_RGBA16x2(r, g, b, a) ((PACK_RGBA16(r, g, b, a) << 16) | PACK_RGBA16(r, g, b, a))

#define PACK_RGBA32(r, g, b, a) (((r & 0xFF) << 24) | ((g & 0xFF) << 16) | ((b & 0xFF) << 8) | (a & 0xFF))

#define PACK_ZDZ(z, dz) (((z << 2) | (dz & 0x3)) & 0xFFFF)
#define PACK_ZDZx2(z, dz) ((PACK_ZDZ(z, dz) << 16) | PACK_ZDZ(z, dz))

#define float_to_fixed(value, frac_bits) ((value) * (1 << (frac_bits)))
#define fixed_to_float(value, frac_bits) ((value) / (1 << (frac_bits)))

#define __ugfx_mask_shift(x, mask, shift) (((uint64_t)(x) & mask) << shift)

#define UGFX_PIXEL_SIZE_4B  0ULL
#define UGFX_PIXEL_SIZE_8B  1ULL
#define UGFX_PIXEL_SIZE_16B 2ULL
#define UGFX_PIXEL_SIZE_32B 3ULL

#define UGFX_FORMAT_RGBA  0ULL
#define UGFX_FORMAT_YUV   1ULL
#define UGFX_FORMAT_INDEX 2ULL
#define UGFX_FORMAT_IA    3ULL
#define UGFX_FORMAT_I     4ULL

#define UGFX_SCISSOR_DEFAULT         0ULL
#define UGFX_SCISSOR_INTERLACED_EVEN 2ULL
#define UGFX_SCISSOR_INTERLACED_ODD  3ULL

#define UGFX_CC_COMBINED_COLOR 0ULL
#define UGFX_CC_T0_COLOR       1ULL
#define UGFX_CC_T1_COLOR       2ULL
#define UGFX_CC_PRIM_COLOR     3ULL
#define UGFX_CC_SHADE_COLOR    4ULL
#define UGFX_CC_ENV_COLOR      5ULL
#define UGFX_CC_KEY_CENTER     6ULL
#define UGFX_CC_KEY_SCALE      6ULL
#define UGFX_CC_COMBINED_ALPHA 7ULL
#define UGFX_CC_T0_ALPHA       8ULL
#define UGFX_CC_T1_ALPHA       9ULL
#define UGFX_CC_PRIM_ALPHA     10ULL
#define UGFX_CC_SHADE_ALPHA    11ULL
#define UGFX_CC_ENV_ALPHA      12ULL
#define UGFX_CC_LOD_FRAC       13ULL
#define UGFX_CC_PRIM_LOD_FRAC  14ULL
#define UGFX_CC_CONVERT_K5     15ULL
#define UGFX_CC_NOISE          7ULL
#define UGFX_CC_CONVERT_K4     7ULL
#define UGFX_CC_1              6ULL
#define UGFX_CC_SUB_0          15ULL
#define UGFX_CC_MUL_0          31ULL
#define UGFX_CC_ADD_0          7ULL

#define UGFX_AC_COMBINED_ALPHA 0ULL
#define UGFX_AC_T0_ALPHA       1ULL
#define UGFX_AC_T1_ALPHA       2ULL
#define UGFX_AC_PRIM_ALPHA     3ULL
#define UGFX_AC_SHADE_ALPHA    4ULL
#define UGFX_AC_ENV_ALPHA      5ULL
#define UGFX_AC_0              6ULL
#define UGFX_AC_1              7ULL
#define UGFX_AC_LOD_FRAC       0ULL
#define UGFX_AC_PRIM_LOD_FRAC  6ULL


#define UGFX_BLEND_IN_RGB      0ULL
#define UGFX_BLEND_MEM_RGB     1ULL
#define UGFX_BLEND_BLEND_RGB   2ULL
#define UGFX_BLEND_FOG_RGB     3ULL

#define UGFX_BLEND_IN_ALPHA    0ULL
#define UGFX_BLEND_FOG_ALPHA   1ULL
#define UGFX_BLEND_SHADE_ALPHA 2ULL
#define UGFX_BLEND_0           3ULL

#define UGFX_BLEND_1_MINUS_A   0ULL
#define UGFX_BLEND_MEM_ALPHA   1ULL
#define UGFX_BLEND_1           2ULL

#define __ugfx_blend_cycle(a, p, b, m) ( \
    __ugfx_mask_shift((a), 0x3, 12) | \
    __ugfx_mask_shift((p), 0x3, 8) | \
    __ugfx_mask_shift((b), 0x3, 4) | \
    __ugfx_mask_shift((m), 0x3, 0))

// TODO: Pre-defined blend modes

#define ugfx_blend_cycles(cycle1, cycle2) ((cycle1) << 18 | (cycle2) << 16)
#define ugfx_blend_2cycle(a1, p1, b1, m1, a2, p2, b2, m2) ugfx_blend_cycles(__ugfx_blend_cycle(a1, p1, b1, m1), __ugfx_blend_cycle(a2, p2, b2, m2))
#define ugfx_blend_1cycle(a, p, b, m) ugfx_blend_cycles(__ugfx_blend_cycle(a, p, b, m), __ugfx_blend_cycle(a, p, b, m))

#define UGFX_ALPHA_COMPARE            1ULL << 0
#define UGFX_DITHER_ALPHA             1ULL << 1
#define UGFX_Z_SOURCE_PIXEL           0ULL << 2
#define UGFX_Z_SOURCE_PRIMITIVE       1ULL << 2
#define UGFX_ANTIALIAS                1ULL << 3
#define UGFX_Z_COMPARE                1ULL << 4
#define UGFX_Z_UPDATE                 1ULL << 5
#define UGFX_IMAGE_READ               1ULL << 6
#define UGFX_COLOR_ON_CVG             1ULL << 7
#define UGFX_CVG_CLAMP                0ULL << 8
#define UGFX_CVG_WRAP                 1ULL << 8
#define UGFX_CVG_ZAP                  2ULL << 8
#define UGFX_CVG_SAVE                 3ULL << 8
#define UGFX_Z_OPAQUE                 0ULL << 10
#define UGFX_Z_INTERPENETRATING       1ULL << 10
#define UGFX_Z_TRANSPARENT            2ULL << 10
#define UGFX_Z_DECAL                  3ULL << 10
#define UGFX_CVG_TIMES_ALPHA          1ULL << 12
#define UGFX_ALPHA_CVG_SELECT         1ULL << 13
#define UGFX_FORCE_BLEND              1ULL << 14
#define UGFX_ALPHA_DITHER_PATTERN     0ULL << 36
#define UGFX_ALPHA_DITHER_INV_PATTERN 1ULL << 36
#define UGFX_ALPHA_DITHER_NOISE       2ULL << 36
#define UGFX_ALPHA_DITHER_NONE        3ULL << 36
#define UGFX_RGB_DITHER_MAGIC         0ULL << 38
#define UGFX_RGB_DITHER_BAYER         1ULL << 38
#define UGFX_RGB_DITHER_NOISE         2ULL << 38
#define UGFX_RGB_DITHER_NONE          3ULL << 38
#define UGFX_KEY_EN                   1ULL << 40
#define UGFX_CONVERT_ONE              1ULL << 41
#define UGFX_BI_LERP_1                1ULL << 42
#define UGFX_BI_LERP_0                1ULL << 43
#define UGFX_MID_TEXEL                1ULL << 44
#define UGFX_SAMPLE_POINT             0ULL << 45
#define UGFX_SAMPLE_2x2               1ULL << 45
#define UGFX_TLUT_OFF                 0ULL << 46
#define UGFX_TLUT_RGBA16              2ULL << 46
#define UGFX_TLUT_IA8                 3ULL << 46
#define UGFX_TEX_LOD                  1ULL << 48
#define UGFX_SHARPEN_TEX              1ULL << 49
#define UGFX_DETAIL_TEX               1ULL << 50
#define UGFX_PERSP_TEX                1ULL << 51
#define UGFX_CYCLE_1CYCLE             0ULL << 52
#define UGFX_CYCLE_2CYCLE             1ULL << 52
#define UGFX_CYCLE_COPY               2ULL << 52
#define UGFX_CYCLE_FILL               3ULL << 52
#define UGFX_ATOMIC_PRIM              1ULL << 55

#define UGFX_MTX_STACK_MODEL     0x00ULL
#define UGFX_MTX_STACK_VIEW_PROJ 0x01ULL
#define UGFX_MTX_FORCE           0x02ULL
#define UGFX_MTX_POP             0x04ULL
#define UGFX_MTX_LOAD            0x00ULL
#define UGFX_MTX_MUL             0x08ULL
#define UGFX_MTX_NOPUSH          0x00ULL
#define UGFX_MTX_PUSH            0x10ULL

#define UGFX_CULL_NONE  0x0ULL
#define UGFX_CULL_BACK  0x1ULL
#define UGFX_CULL_FRONT 0x2ULL
#define UGFX_CULL_BOTH  0x3ULL

#define UGFX_GEOMETRY_FILL     0x00ULL
#define UGFX_GEOMETRY_ZBUFFER  0x01ULL
#define UGFX_GEOMETRY_TEXTURE  0x02ULL
#define UGFX_GEOMETRY_SHADE    0x04ULL
#define UGFX_GEOMETRY_SMOOTH   0x08ULL
#define UGFX_GEOMETRY_LIGHTING 0x10ULL
#define UGFX_GEOMETRY_FULL     UGFX_GEOMETRY_ZBUFFER | UGFX_GEOMETRY_TEXTURE | UGFX_GEOMETRY_SHADE | UGFX_GEOMETRY_SMOOTH | UGFX_GEOMETRY_LIGHTING

#define UGFX_LINK_PUSH 0x0ULL
#define UGFX_LINK_LOAD 0x1ULL

#define UGFX_OP_NOOP                   0x00ULL
#define UGFX_OP_TEXTURE_RECTANGLE      0x24ULL
#define UGFX_OP_TEXTURE_RECTANGLE_FLIP 0x25ULL
#define UGFX_OP_SYNC_PIPE              0x27ULL
#define UGFX_OP_SYNC_TILE              0x28ULL
#define UGFX_OP_SYNC_FULL              0x29ULL
#define UGFX_OP_SET_KEY_GB             0x2AULL
#define UGFX_OP_SET_KEY_R              0x2BULL
#define UGFX_OP_SET_CONVERT            0x2CULL
#define UGFX_OP_SET_SCISSOR            0x2DULL
#define UGFX_OP_SET_PRIM_DEPTH         0x2EULL
#define UGFX_OP_SET_OTHER_MODES        0x2FULL
#define UGFX_OP_LOAD_TLUT              0x30ULL
#define UGFX_OP_SYNC_LOAD              0x31ULL
#define UGFX_OP_SET_TILE_SIZE          0x32ULL
#define UGFX_OP_LOAD_BLOCK             0x33ULL
#define UGFX_OP_LOAD_TILE              0x34ULL
#define UGFX_OP_SET_TILE               0x35ULL
#define UGFX_OP_FILL_RECTANGLE         0x36ULL
#define UGFX_OP_SET_FILL_COLOR         0x37ULL
#define UGFX_OP_SET_FOG_COLOR          0x38ULL
#define UGFX_OP_SET_BLEND_COLOR        0x39ULL
#define UGFX_OP_SET_PRIM_COLOR         0x3AULL
#define UGFX_OP_SET_ENV_COLOR          0x3BULL
#define UGFX_OP_SET_COMBINE_MODE       0x3CULL
#define UGFX_OP_SET_TEXTURE_IMAGE      0x3DULL
#define UGFX_OP_SET_Z_IMAGE            0x3EULL
#define UGFX_OP_SET_COLOR_IMAGE        0x3FULL
#define UGFX_OP_FINALIZE               0x80ULL
#define UGFX_OP_LOAD_VERTICES          0x81ULL
#define UGFX_OP_LOAD_MATRIX            0x82ULL
#define UGFX_OP_LOAD_VIEWPORT          0x83ULL
#define UGFX_OP_LOAD_LIGHT             0x84ULL
#define UGFX_OP_LINK_COMMANDS          0x85ULL
#define UGFX_OP_SET_CULL_MODE          0x86ULL
#define UGFX_OP_SET_GEOMETRY_MODE      0x87ULL
#define UGFX_OP_SET_PERSP_NORM         0x88ULL
#define UGFX_OP_SET_TEXTURE_SETTINGS   0x89ULL
#define UGFX_OP_SET_NUM_LIGHTS         0x8AULL
#define UGFX_OP_SET_CLIP_RATIO         0x8BULL
#define UGFX_OP_SET_ADDRESS_SLOT       0x8CULL
#define UGFX_OP_DRAW_TRIANGLE          0x8DULL

#define __ugfx_opcode(opcode) __ugfx_mask_shift((opcode), 0xFF, 56)

#define ugfx_noop() (__ugfx_opcode(UGFX_OP_NOOP))

#define ugfx_texture_rectangle(tile, xh, yh, xl, yl) ( \
    __ugfx_opcode(UGFX_OP_TEXTURE_RECTANGLE) | \
    __ugfx_mask_shift((xl), 0xFFF, 44) | \
    __ugfx_mask_shift((yl), 0xFFF, 32) | \
    __ugfx_mask_shift((tile), 0x7, 24) | \
    __ugfx_mask_shift((xh), 0xFFF, 12) | \
    __ugfx_mask_shift((yh), 0xFFF, 0))

#define ugfx_texture_rectangle_flip(tile, xh, yh, xl, yl) ( \
    __ugfx_opcode(UGFX_OP_TEXTURE_RECTANGLE_FLIP) | \
    __ugfx_mask_shift((xl), 0xFFF, 44) | \
    __ugfx_mask_shift((yl), 0xFFF, 32) | \
    __ugfx_mask_shift((tile), 0x7, 24) | \
    __ugfx_mask_shift((xh), 0xFFF, 12) | \
    __ugfx_mask_shift((yh), 0xFFF, 0))

#define ugfx_texture_rectangle_tcoords(s, t, dsdx, dtdy) ( \
    __ugfx_mask_shift((s), 0xFFFF, 48) | \
    __ugfx_mask_shift((t), 0xFFFF, 32) | \
    __ugfx_mask_shift((dsdx), 0xFFFF, 16) | \
    __ugfx_mask_shift((dtdy), 0xFFFF, 0))

#define ugfx_sync_pipe() __ugfx_opcode(UGFX_OP_SYNC_PIPE)
#define ugfx_sync_tile() __ugfx_opcode(UGFX_OP_SYNC_TILE)
#define ugfx_sync_full() __ugfx_opcode(UGFX_OP_SYNC_FULL)
#define ugfx_sync_load() __ugfx_opcode(UGFX_OP_SYNC_LOAD)

#define ugfx_set_key_gb(width_g, center_g, scale_g, width_b, center_b, scale_b) ( \
    __ugfx_opcode(UGFX_OP_SET_KEY_GB) | \
    __ugfx_mask_shift((width_g), 0xFFF, 44) | \
    __ugfx_mask_shift((width_b), 0xFFF, 32) | \
    __ugfx_mask_shift((center_g), 0xFF, 24) | \
    __ugfx_mask_shift((scale_g), 0xFF, 16) | \
    __ugfx_mask_shift((center_b), 0xFF, 8) | \
    __ugfx_mask_shift((scale_b), 0xFF, 0))

#define ugfx_set_key_r(width_r, center_r, scale_r) ( \
    __ugfx_opcode(UGFX_OP_SET_KEY_R) | \
    __ugfx_mask_shift((width_r), 0xFFF, 16) | \
    __ugfx_mask_shift((center_r), 0xFF, 8) | \
    __ugfx_mask_shift((scale_r), 0xFF, 0))

#define ugfx_set_convert(k0, k1, k2, k3, k4, k5) ( \
    __ugfx_opcode(UGFX_OP_SET_CONVERT) | \
    __ugfx_mask_shift((k0), 0x1FF, 45) | \
    __ugfx_mask_shift((k1), 0x1FF, 36) | \
    __ugfx_mask_shift((k2), 0x1FF, 27) | \
    __ugfx_mask_shift((k3), 0x1FF, 18) | \
    __ugfx_mask_shift((k4), 0x1FF, 9) | \
    __ugfx_mask_shift((k5), 0x1FF, 0))

#define ugfx_set_scissor(xh, yh, xl, yl, mode) ( \
    __ugfx_opcode(UGFX_OP_SET_SCISSOR) | \
    __ugfx_mask_shift((xh), 0xFFF, 44) | \
    __ugfx_mask_shift((yh), 0xFFF, 32) | \
    __ugfx_mask_shift((mode), 0x3, 24) | \
    __ugfx_mask_shift((xl), 0xFFF, 12) | \
    __ugfx_mask_shift((yl), 0xFFF, 0))

#define ugfx_set_prim_depth(primitive_z, primitive_delta_z) ( \
    __ugfx_opcode(UGFX_OP_SET_PRIM_DEPTH) | \
    __ugfx_mask_shift((primitive_z), 0xFFFF, 16) | \
    __ugfx_mask_shift((primitive_delta_z), 0xFFFF, 0))

#define ugfx_set_other_modes(flags) ( \
    __ugfx_opcode(UGFX_OP_SET_OTHER_MODES) | \
    __ugfx_mask_shift((flags), 0xFFFFFFFFFFFFFF, 0))

#define ugfx_load_tlut(sl, tl, sh, th, tile) ( \
    __ugfx_opcode(UGFX_OP_LOAD_TLUT) | \
    __ugfx_mask_shift((sl), 0xFFF, 44) | \
    __ugfx_mask_shift((tl), 0xFFF, 32) | \
    __ugfx_mask_shift((tile), 0x7, 24) | \
    __ugfx_mask_shift((sh), 0xFFF, 12) | \
    __ugfx_mask_shift((th), 0xFFF, 0))

#define ugfx_set_tile_size(sl, tl, sh, th, tile) ( \
    __ugfx_opcode(UGFX_OP_SET_TILE_SIZE) | \
    __ugfx_mask_shift((sl), 0xFFF, 44) | \
    __ugfx_mask_shift((tl), 0xFFF, 32) | \
    __ugfx_mask_shift((tile), 0x7, 24) | \
    __ugfx_mask_shift((sh), 0xFFF, 12) | \
    __ugfx_mask_shift((th), 0xFFF, 0))

#define ugfx_load_block(sl, tl, sh, dxt, tile) ( \
    __ugfx_opcode(UGFX_OP_LOAD_BLOCK) | \
    __ugfx_mask_shift((sl), 0xFFF, 44) | \
    __ugfx_mask_shift((tl), 0xFFF, 32) | \
    __ugfx_mask_shift((tile), 0x7, 24) | \
    __ugfx_mask_shift((sh), 0xFFF, 12) | \
    __ugfx_mask_shift((dxt), 0xFFF, 0))

#define ugfx_load_tile(sl, tl, sh, th, tile) ( \
    __ugfx_opcode(UGFX_OP_LOAD_TILE) | \
    __ugfx_mask_shift((sl), 0xFFF, 44) | \
    __ugfx_mask_shift((tl), 0xFFF, 32) | \
    __ugfx_mask_shift((tile), 0x7, 24) | \
    __ugfx_mask_shift((sh), 0xFFF, 12) | \
    __ugfx_mask_shift((th), 0xFFF, 0))

#define ugfx_set_tile(format, size, line, tmem_addr, tile, palette, ct, mt, mask_t, shift_t, cs, ms, mask_s, shift_s) ( \
    __ugfx_opcode(UGFX_OP_SET_TILE) | \
    __ugfx_mask_shift((format), 0x7, 53) | \
    __ugfx_mask_shift((size), 0x3, 51) | \
    __ugfx_mask_shift((line), 0x1FF, 41) | \
    __ugfx_mask_shift((tmem_addr), 0x1FF, 32) | \
    __ugfx_mask_shift((tile), 0x7, 24) | \
    __ugfx_mask_shift((palette), 0xF, 20) | \
    __ugfx_mask_shift((ct), 0x1, 19) | \
    __ugfx_mask_shift((mt), 0x1, 18) | \
    __ugfx_mask_shift((mask_t), 0xF, 14) | \
    __ugfx_mask_shift((shift_t), 0xF, 10) | \
    __ugfx_mask_shift((cs), 0x1, 9) | \
    __ugfx_mask_shift((ms), 0x1, 8) | \
    __ugfx_mask_shift((mask_s), 0xFF, 4) | \
    __ugfx_mask_shift((shift_s), 0xF, 0))

#define ugfx_fill_rectangle(xh, yh, xl, yl) ( \
    __ugfx_opcode(UGFX_OP_FILL_RECTANGLE) | \
    __ugfx_mask_shift((xl), 0xFFF, 44) | \
    __ugfx_mask_shift((yl), 0xFFF, 32) | \
    __ugfx_mask_shift((xh), 0xFFF, 12) | \
    __ugfx_mask_shift((yh), 0xFFF, 0))

#define ugfx_set_fill_color(packed_color) ( \
    __ugfx_opcode(UGFX_OP_SET_FILL_COLOR) | \
    __ugfx_mask_shift((packed_color), 0xFFFFFFFF, 0))

#define ugfx_set_fog_color(rgba32) ( \
    __ugfx_opcode(UGFX_OP_SET_FOG_COLOR) | \
    __ugfx_mask_shift((rgba32), 0xFFFFFFFF, 0))

#define ugfx_set_blend_color(rgba32) ( \
    __ugfx_opcode(UGFX_OP_SET_BLEND_COLOR) | \
    __ugfx_mask_shift((rgba32), 0xFFFFFFFF, 0))

#define ugfx_set_prim_color(prim_min_level, prim_level_frac, rgba32) ( \
    __ugfx_opcode(UGFX_OP_SET_PRIM_COLOR) | \
    __ugfx_mask_shift((prim_min_level), 0x1F, 40) | \
    __ugfx_mask_shift((prim_level_frac), 0xFF, 32) | \
    __ugfx_mask_shift((rgba32), 0xFFFFFFFF, 0))

#define ugfx_set_env_color(rgba32) ( \
    __ugfx_opcode(UGFX_OP_SET_ENV_COLOR) | \
    __ugfx_mask_shift((rgba32), 0xFFFFFFFF, 0))

#define ugfx_set_combine_mode(sub_a_r0, sub_b_r0, mul_r0, add_r0, sub_a_a0, sub_b_a0, mul_a0, add_a0, sub_a_r1, sub_b_r1, mul_r1, add_r1, sub_a_a1, sub_b_a1, mul_a1, add_a1) ( \
    __ugfx_opcode(UGFX_OP_SET_COMBINE_MODE) | \
    __ugfx_mask_shift((sub_a_r0), 0xF, 52) | \
    __ugfx_mask_shift((mul_r0), 0x1F, 47) | \
    __ugfx_mask_shift((sub_a_a0), 0x7, 44) | \
    __ugfx_mask_shift((mul_a0), 0x7, 41) | \
    __ugfx_mask_shift((sub_a_r1), 0xF, 37) | \
    __ugfx_mask_shift((mul_r1), 0x1F, 32) | \
    __ugfx_mask_shift((sub_b_r0), 0xF, 28) | \
    __ugfx_mask_shift((sub_b_r1), 0xF, 24) | \
    __ugfx_mask_shift((sub_a_a1), 0x7, 21) | \
    __ugfx_mask_shift((mul_a1), 0x7, 18) | \
    __ugfx_mask_shift((add_r0), 0x7, 15) | \
    __ugfx_mask_shift((sub_b_a0), 0x7, 12) | \
    __ugfx_mask_shift((add_a0), 0x7, 9) | \
    __ugfx_mask_shift((add_r1), 0x7, 6) | \
    __ugfx_mask_shift((sub_b_a1), 0x7, 3) | \
    __ugfx_mask_shift((add_a1), 0x7, 0))

#define ugfx_set_texture_image(dram_addr, format, size, width) ( \
    __ugfx_opcode(UGFX_OP_SET_TEXTURE_IMAGE) | \
    __ugfx_mask_shift((format), 0x7, 53) | \
    __ugfx_mask_shift((size), 0x3, 51) | \
    __ugfx_mask_shift((width), 0x1FFFF, 32) | \
    __ugfx_mask_shift((uintptr_t)(dram_addr), 0x1FFFFFF, 0))

#define ugfx_set_z_image(dram_addr) ( \
    __ugfx_opcode(UGFX_OP_SET_Z_IMAGE) | \
    __ugfx_mask_shift((uintptr_t)(dram_addr), 0x1FFFFFF, 0))

#define ugfx_set_color_image(dram_addr, format, size, width) ( \
    __ugfx_opcode(UGFX_OP_SET_COLOR_IMAGE) | \
    __ugfx_mask_shift((format), 0x7, 53) | \
    __ugfx_mask_shift((size), 0x3, 51) | \
    __ugfx_mask_shift((width), 0x3FF, 32) | \
    __ugfx_mask_shift((uintptr_t)(dram_addr), 0x1FFFFFF, 0))

#define ugfx_finalize() __ugfx_opcode(UGFX_OP_FINALIZE)

#define ugfx_load_vertices(slot, vertices, index, count) ( \
    __ugfx_opcode(UGFX_OP_LOAD_VERTICES) | \
    __ugfx_mask_shift((count), 0x3F, 44) | \
    __ugfx_mask_shift((index), 0x3F, 36) | \
    __ugfx_mask_shift((slot), 0xF, 28) | \
    __ugfx_mask_shift((uintptr_t)(vertices), 0x1FFFFFF, 0))

#define ugfx_load_matrix(slot, matrix, flags) ( \
    __ugfx_opcode(UGFX_OP_LOAD_MATRIX) | \
    __ugfx_mask_shift((flags), 0xFFFFFF, 32) | \
    __ugfx_mask_shift((slot), 0xF, 28) | \
    __ugfx_mask_shift((uintptr_t)(matrix), 0x1FFFFFF, 0))

#define ugfx_load_viewport(slot, viewport) (\
    __ugfx_opcode(UGFX_OP_LOAD_VIEWPORT) | \
    __ugfx_mask_shift((slot), 0xF, 28) | \
    __ugfx_mask_shift((uintptr_t)(viewport), 0x1FFFFFF, 0))

#define ugfx_load_light(slot, light, index) (\
    __ugfx_opcode(UGFX_OP_LOAD_LIGHT) | \
    __ugfx_mask_shift((index), 0x7, 35) | \
    __ugfx_mask_shift((slot), 0xF, 28) | \
    __ugfx_mask_shift((uintptr_t)(light), 0x1FFFFFF, 0))

#define ugfx_link_commands(slot, commands, length, flags) (\
    __ugfx_opcode(UGFX_OP_LINK_COMMANDS) | \
    __ugfx_mask_shift((length), 0x1FFFFF, 35) | \
    __ugfx_mask_shift((flags), 0x7, 32) | \
    __ugfx_mask_shift((slot), 0xF, 28) | \
    __ugfx_mask_shift((uintptr_t)(commands), 0x1FFFFFF, 0))

#define ugfx_set_cull_mode(mode) (\
    __ugfx_opcode(UGFX_OP_SET_CULL_MODE) | \
    __ugfx_mask_shift((mode), 0x3, 32))

#define ugfx_set_geometry_mode(mode) (\
    __ugfx_opcode(UGFX_OP_SET_GEOMETRY_MODE) | \
    __ugfx_mask_shift((mode), 0xFF, 32))

#define ugfx_set_persp_norm(scale) (\
    __ugfx_opcode(UGFX_OP_SET_PERSP_NORM) | \
    __ugfx_mask_shift((scale), 0xFFFF, 32))

#define ugfx_set_texture_settings(scale_s, scale_t, level, tile) (\
    __ugfx_opcode(UGFX_OP_SET_TEXTURE_SETTINGS) | \
    __ugfx_mask_shift((level), 0x7, 35) | \
    __ugfx_mask_shift((tile), 0x7, 32) | \
    __ugfx_mask_shift((scale_s), 0xFFFF, 16) | \
    __ugfx_mask_shift((scale_t), 0xFFFF, 0))

#define ugfx_set_num_lights(num) (\
    __ugfx_opcode(UGFX_OP_SET_NUM_LIGHTS) | \
    __ugfx_mask_shift((num), 0x7, 35))

#define ugfx_set_clip_ratio(ratio) (\
    __ugfx_opcode(UGFX_OP_SET_CLIP_RATIO) | \
    __ugfx_mask_shift((ratio), 0xFFFF, 32))

#define ugfx_set_address_slot(slot, address) (\
    __ugfx_opcode(UGFX_OP_SET_ADDRESS_SLOT) | \
    __ugfx_mask_shift((slot), 0xF, 28) | \
    __ugfx_mask_shift((uintptr_t)(address), 0x1FFFFFF, 0))

#define ugfx_draw_triangle(v0, v1, v2) ( \
    __ugfx_opcode(UGFX_OP_DRAW_TRIANGLE) | \
    __ugfx_mask_shift((v0), 0x3F, 49) | \
    __ugfx_mask_shift((v1), 0x3F, 43) | \
    __ugfx_mask_shift((v2), 0x3F, 37))

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ugfx_vertex_t {
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t padding;
    int16_t s;
    int16_t t;

    union {
        uint32_t rgba;

        struct {
          uint8_t r;
          uint8_t g;
          uint8_t b;
          uint8_t a;
        } color;

        struct {
          int8_t x;
          int8_t y;
          int8_t z;
          uint8_t a;
        } normal;
    } attr;
} __attribute__((aligned(16))) ugfx_vertex_t;

typedef struct ugfx_matrix_t {
    int16_t integer[4][4];
    uint16_t fraction[4][4];
} __attribute__((aligned(16))) ugfx_matrix_t;

void ugfx_matrix_from_column_major(ugfx_matrix_t *dest, const float *source);
void ugfx_matrix_from_row_major(ugfx_matrix_t *dest, const float *source);

typedef struct ugfx_viewport_t {
    int16_t scale[4];
    int16_t offset[4];
} __attribute__((aligned(16))) ugfx_viewport_t;

typedef struct ugfx_light_t {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t padding0;
    int8_t x;
    int8_t y;
    int8_t z;
    uint8_t padding1;
} __attribute__((aligned(16))) ugfx_light_t;

typedef uint64_t ugfx_command_t;

typedef struct ugfx_buffer_t {
    ugfx_command_t *data;
    uint32_t capacity;
    uint32_t length;
} ugfx_buffer_t;

static inline float get_persp_norm_scale(float near, float far)
{
    return 2.0f / (far + near);
}


static inline ugfx_command_t* ugfx_buffer_data(const ugfx_buffer_t *buffer)
{
    return buffer->data;
}

static inline uint32_t ugfx_buffer_length(const ugfx_buffer_t *buffer)
{
    return buffer->length;
}

static inline bool ugfx_buffer_is_empty(const ugfx_buffer_t *buffer)
{
    return ugfx_buffer_length(buffer) <= 0;
}

ugfx_buffer_t* ugfx_buffer_new(uint32_t capacity);
void ugfx_buffer_free(ugfx_buffer_t *buffer);

void ugfx_buffer_clear(ugfx_buffer_t *buffer);
void ugfx_buffer_push(ugfx_buffer_t *buffer, ugfx_command_t command);
void ugfx_buffer_insert(ugfx_buffer_t *buffer, const ugfx_command_t *commands, uint32_t count);

void ugfx_init(uint32_t rdp_buffer_size);
void ugfx_load(const ugfx_command_t *commands, uint32_t commands_length);
void ugfx_close();

ugfx_command_t ugfx_set_display(display_context_t disp);

static inline void ugfx_load_buffer(const ugfx_buffer_t *buffer)
{
    ugfx_load(buffer->data, buffer->length);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
