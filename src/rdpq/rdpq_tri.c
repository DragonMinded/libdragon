/**
 * @file rdpq_tri.c
 * @brief RDP Command queue: triangle drawing routine
 * @ingroup rdp
 * 
 * This file contains the implementation of a single function: #rdpq_triangle.
 * 
 * The RDP triangle commands are complex to assemble because they are designed
 * for the hardware that will be drawing them, rather than for the programmer
 * that needs to create them. Specifically, they contain explicit gradients
 * (partial derivatives aka horizontal and vertical per-pixel increments)
 * for all attributes that need to be interpolated. Moreover, the RDP is able
 * to draw triangles with subpixel precision, so input coordinates are fixed
 * point and the setup code must take into account exactly how the rasterizer
 * will handle fractional values.
 */

#include <math.h>
#include <float.h>
#include "rdpq.h"
#include "rdpq_tri.h"
#include "rspq.h"
#include "rdpq_internal.h"
#include "rdpq_constants.h"
#include "utils.h"
#include "debug.h"

/** @brief Set to 1 to activate tracing of all parameters of all triangles. */
#define TRIANGLE_TRACE   0

#if TRIANGLE_TRACE
/** @brief like debugf(), but writes only if #TRIANGLE_TRACE is not 0 */
#define tracef(fmt, ...)  debugf(fmt, ##__VA_ARGS__)
#else
/** @brief like debugf(), but writes only if #TRIANGLE_TRACE is not 0 */
#define tracef(fmt, ...)  ({ })
#endif

const rdpq_trifmt_t TRIFMT_FILL = (rdpq_trifmt_t){
    .pos_offset = 0, .shade_offset = -1, .tex_offset = -1, .z_offset = -1,
};

const rdpq_trifmt_t TRIFMT_SHADE = (rdpq_trifmt_t){
    .pos_offset = 0, .shade_offset = 2, .tex_offset = -1, .z_offset = -1,
};

const rdpq_trifmt_t TRIFMT_TEX = (rdpq_trifmt_t){
    .pos_offset = 0, .shade_offset = -1, .tex_offset = 2, .z_offset = -1,
};

const rdpq_trifmt_t TRIFMT_SHADE_TEX = (rdpq_trifmt_t){
    .pos_offset = 0, .shade_offset = 2, .tex_offset = 6, .z_offset = -1,
};

const rdpq_trifmt_t TRIFMT_ZBUF = (rdpq_trifmt_t){
    .pos_offset = 0, .shade_offset = -1, .tex_offset = -1, .z_offset = 2,
};

const rdpq_trifmt_t TRIFMT_ZBUF_SHADE = (rdpq_trifmt_t){
    .pos_offset = 0, .shade_offset = 3, .tex_offset = -1, .z_offset = 2,
};

const rdpq_trifmt_t TRIFMT_ZBUF_TEX = (rdpq_trifmt_t){
    .pos_offset = 0, .shade_offset = -1, .tex_offset = 3, .z_offset = 2,
};

const rdpq_trifmt_t TRIFMT_ZBUF_SHADE_TEX = (rdpq_trifmt_t){
    .pos_offset = 0, .shade_offset = 3, .tex_offset = 7, .z_offset = 2,
};

/** @brief Converts a float to a s16.16 fixed point number */
static int32_t float_to_s16_16(float f)
{
    // Currently the float must be clamped to this range because
    // otherwise the trunc.w.s instruction can potentially trigger
    // an unimplemented operation exception due to integer overflow.
    // TODO: maybe handle the exception? Clamp the value in the exception handler?
    if (f >= 32768.f) {
        return 0x7FFFFFFF;
    }

    if (f < -32768.f) {
        return 0x80000000;
    }

    return floor(f * 65536.f);
}

/** @brief Precomputed information about edges and slopes. */
typedef struct {
    float hx;               ///< High edge (X)
    float hy;               ///< High edge (Y)
    float mx;               ///< Middle edge (X)
    float my;               ///< Middle edge (Y)
    float fy;               ///< Fractional part of Y1 (top vertex)
    float ish;              ///< Inverse slope of higher edge
    float attr_factor;      ///< Inverse triangle normal (used to calculate gradients)
} rdpq_tri_edge_data_t;

__attribute__((always_inline))
static inline void __rdpq_write_edge_coeffs(rspq_write_t *w, rdpq_tri_edge_data_t *data, uint8_t tile, uint8_t mipmaps, const float *v1, const float *v2, const float *v3)
{
    const float x1 = v1[0];
    const float x2 = v2[0];
    const float x3 = v3[0];
    const float y1 = floorf(v1[1]*4)/4;
    const float y2 = floorf(v2[1]*4)/4;
    const float y3 = floorf(v3[1]*4)/4;

    const float to_fixed_11_2 = 4.0f;
    int32_t y1f = CLAMP((int32_t)floorf(v1[1]*to_fixed_11_2), -4096*4, 4095*4);
    int32_t y2f = CLAMP((int32_t)floorf(v2[1]*to_fixed_11_2), -4096*4, 4095*4);
    int32_t y3f = CLAMP((int32_t)floorf(v3[1]*to_fixed_11_2), -4096*4, 4095*4);

    data->hx = x3 - x1;
    data->hy = y3 - y1;
    data->mx = x2 - x1;
    data->my = y2 - y1;
    float lx = x3 - x2;
    float ly = y3 - y2;

    const float nz = (data->hx*data->my) - (data->hy*data->mx);
    data->attr_factor = (fabs(nz) > FLT_MIN) ? (-1.0f / nz) : 0;
    const uint32_t lft = nz < 0;

    data->ish = (fabs(data->hy) > FLT_MIN) ? (data->hx / data->hy) : 0;
    float ism = (fabs(data->my) > FLT_MIN) ? (data->mx / data->my) : 0;
    float isl = (fabs(ly) > FLT_MIN) ? (lx / ly) : 0;
    data->fy = floorf(y1) - y1;

    const float xh = x1 + data->fy * data->ish;
    const float xm = x1 + data->fy * ism;
    const float xl = x2;

    rspq_write_arg(w, _carg(lft, 0x1, 23) | _carg(mipmaps ? mipmaps-1 : 0, 0x7, 19) | _carg(tile, 0x7, 16) | _carg(y3f, 0x3FFF, 0));
    rspq_write_arg(w, _carg(y2f, 0x3FFF, 16) | _carg(y1f, 0x3FFF, 0));
    rspq_write_arg(w, float_to_s16_16(xl));
    rspq_write_arg(w, float_to_s16_16(isl));
    rspq_write_arg(w, float_to_s16_16(xh));
    rspq_write_arg(w, float_to_s16_16(data->ish));
    rspq_write_arg(w, float_to_s16_16(xm));
    rspq_write_arg(w, float_to_s16_16(ism));

    tracef("x1:  %f (%08lx)\n", x1, (int32_t)(x1 * 4.0f));
    tracef("x2:  %f (%08lx)\n", x2, (int32_t)(x2 * 4.0f));
    tracef("x3:  %f (%08lx)\n", x3, (int32_t)(x3 * 4.0f));
    tracef("y1:  %f (%08lx)\n", y1, (int32_t)(y1 * 4.0f));
    tracef("y2:  %f (%08lx)\n", y2, (int32_t)(y2 * 4.0f));
    tracef("y3:  %f (%08lx)\n", y3, (int32_t)(y3 * 4.0f));

    tracef("hx:  %f (%08lx)\n", data->hx, (int32_t)(data->hx * 4.0f));
    tracef("hy:  %f (%08lx)\n", data->hy, (int32_t)(data->hy * 4.0f));
    tracef("mx:  %f (%08lx)\n", data->mx, (int32_t)(data->mx * 4.0f));
    tracef("my:  %f (%08lx)\n", data->my, (int32_t)(data->my * 4.0f));
    tracef("lx:  %f (%08lx)\n", lx, (int32_t)(lx * 4.0f));
    tracef("ly:  %f (%08lx)\n", ly, (int32_t)(ly * 4.0f));

    tracef("p1: %f (%08lx)\n", (data->hx*data->my), (int32_t)(data->hx*data->my*16.0f));
    tracef("p2: %f (%08lx)\n", (data->hy*data->mx), (int32_t)(data->hy*data->mx*16.0f));
    tracef("nz: %f (%08lx)\n", nz, (int32_t)(nz * 16.0f));
    tracef("-nz: %f (%08lx)\n", -nz, -(int32_t)(nz * 16.0f));
    tracef("inv_nz: %f (%08lx)\n", data->attr_factor, (int32_t)(data->attr_factor * 65536.0f / 2.0f / 16.0f));
    
    tracef("fy:  %f (%08lx)\n", data->fy, (int32_t)(data->fy * 65536.0f));
    tracef("ish: %f (%08lx)\n", data->ish, (int32_t)(data->ish * 65536.0f));
    tracef("ism: %f (%08lx)\n", ism, (int32_t)(ism * 65536.0f));
    tracef("isl: %f (%08lx)\n", isl, (int32_t)(isl * 65536.0f));

    tracef("xh: %f (%08lx)\n", xh, (int32_t)(xh * 65536.0f));
    tracef("xm: %f (%08lx)\n", xm, (int32_t)(xm * 65536.0f));
    tracef("xl: %f (%08lx)\n", xl, (int32_t)(xl * 65536.0f));
}

__attribute__((always_inline))
static inline void __rdpq_write_shade_coeffs(rspq_write_t *w, rdpq_tri_edge_data_t *data, const float *v1, const float *v2, const float *v3)
{
    const float mr = (v2[0] - v1[0]) * 255.f;
    const float mg = (v2[1] - v1[1]) * 255.f;
    const float mb = (v2[2] - v1[2]) * 255.f;
    const float ma = (v2[3] - v1[3]) * 255.f;
    const float hr = (v3[0] - v1[0]) * 255.f;
    const float hg = (v3[1] - v1[1]) * 255.f;
    const float hb = (v3[2] - v1[2]) * 255.f;
    const float ha = (v3[3] - v1[3]) * 255.f;

    const float nxR = data->hy*mr - data->my*hr;
    const float nxG = data->hy*mg - data->my*hg;
    const float nxB = data->hy*mb - data->my*hb;
    const float nxA = data->hy*ma - data->my*ha;
    const float nyR = data->mx*hr - data->hx*mr;
    const float nyG = data->mx*hg - data->hx*mg;
    const float nyB = data->mx*hb - data->hx*mb;
    const float nyA = data->mx*ha - data->hx*ma;

    const float DrDx = nxR * data->attr_factor;
    const float DgDx = nxG * data->attr_factor;
    const float DbDx = nxB * data->attr_factor;
    const float DaDx = nxA * data->attr_factor;
    const float DrDy = nyR * data->attr_factor;
    const float DgDy = nyG * data->attr_factor;
    const float DbDy = nyB * data->attr_factor;
    const float DaDy = nyA * data->attr_factor;

    const float DrDe = DrDy + DrDx * data->ish;
    const float DgDe = DgDy + DgDx * data->ish;
    const float DbDe = DbDy + DbDx * data->ish;
    const float DaDe = DaDy + DaDx * data->ish;

    const int32_t final_r = float_to_s16_16(v1[0] * 255.f + data->fy * DrDe);
    const int32_t final_g = float_to_s16_16(v1[1] * 255.f + data->fy * DgDe);
    const int32_t final_b = float_to_s16_16(v1[2] * 255.f + data->fy * DbDe);
    const int32_t final_a = float_to_s16_16(v1[3] * 255.f + data->fy * DaDe);

    const int32_t DrDx_fixed = float_to_s16_16(DrDx);
    const int32_t DgDx_fixed = float_to_s16_16(DgDx);
    const int32_t DbDx_fixed = float_to_s16_16(DbDx);
    const int32_t DaDx_fixed = float_to_s16_16(DaDx);

    const int32_t DrDe_fixed = float_to_s16_16(DrDe);
    const int32_t DgDe_fixed = float_to_s16_16(DgDe);
    const int32_t DbDe_fixed = float_to_s16_16(DbDe);
    const int32_t DaDe_fixed = float_to_s16_16(DaDe);

    const int32_t DrDy_fixed = float_to_s16_16(DrDy);
    const int32_t DgDy_fixed = float_to_s16_16(DgDy);
    const int32_t DbDy_fixed = float_to_s16_16(DbDy);
    const int32_t DaDy_fixed = float_to_s16_16(DaDy);

    rspq_write_arg(w, (final_r&0xffff0000) | (0xffff&(final_g>>16)));
    rspq_write_arg(w, (final_b&0xffff0000) | (0xffff&(final_a>>16)));
    rspq_write_arg(w, (DrDx_fixed&0xffff0000) | (0xffff&(DgDx_fixed>>16)));
    rspq_write_arg(w, (DbDx_fixed&0xffff0000) | (0xffff&(DaDx_fixed>>16)));
    rspq_write_arg(w, (final_r<<16) | (final_g&0xffff));
    rspq_write_arg(w, (final_b<<16) | (final_a&0xffff));
    rspq_write_arg(w, (DrDx_fixed<<16) | (DgDx_fixed&0xffff));
    rspq_write_arg(w, (DbDx_fixed<<16) | (DaDx_fixed&0xffff));
    rspq_write_arg(w, (DrDe_fixed&0xffff0000) | (0xffff&(DgDe_fixed>>16)));
    rspq_write_arg(w, (DbDe_fixed&0xffff0000) | (0xffff&(DaDe_fixed>>16)));
    rspq_write_arg(w, (DrDy_fixed&0xffff0000) | (0xffff&(DgDy_fixed>>16)));
    rspq_write_arg(w, (DbDy_fixed&0xffff0000) | (0xffff&(DaDy_fixed>>16)));
    rspq_write_arg(w, (DrDe_fixed<<16) | (DgDe_fixed&0xffff));
    rspq_write_arg(w, (DbDe_fixed<<16) | (DaDe_fixed&0xffff));
    rspq_write_arg(w, (DrDy_fixed<<16) | (DgDy_fixed&0xffff));
    rspq_write_arg(w, (DbDy_fixed<<16) | (DaDy_fixed&0xffff));

    tracef("b1: %f (%08lx)\n", v1[2], (uint32_t)(v1[2]*255.0f));
    tracef("b2: %f (%08lx)\n", v2[2], (uint32_t)(v2[2]*255.0f));
    tracef("b3: %f (%08lx)\n", v3[2], (uint32_t)(v3[2]*255.0f));
    tracef("mb: %f (%08lx)\n", mb, (uint32_t)(int32_t)mb);
    tracef("hb: %f (%08lx)\n", hb, (uint32_t)(int32_t)hb);
    tracef("nxB: %f (%08lx)\n", nxB, (int32_t)(nxB * 4.0f));
    tracef("DbDx: %f (%08lx)\n", DbDx, (uint32_t)(DbDx * 65536.0f));
    tracef("DbDx_fixed: (%08lx)\n", DbDx_fixed);
}

__attribute__((always_inline))
static inline void __rdpq_write_tex_coeffs(rspq_write_t *w, rdpq_tri_edge_data_t *data, const float *v1, const float *v2, const float *v3)
{
    float s1 = v1[0] * 32.f, t1 = v1[1] * 32.f, invw1 = v1[2];
    float s2 = v2[0] * 32.f, t2 = v2[1] * 32.f, invw2 = v2[2];
    float s3 = v3[0] * 32.f, t3 = v3[1] * 32.f, invw3 = v3[2];

    const float minw = 1.0f / MAX(MAX(invw1, invw2), invw3);

    tracef("s1: %f (%04x)\n", s1, (int16_t)s1);
    tracef("t1: %f (%04x)\n", t1, (int16_t)t1);
    tracef("s2: %f (%04x)\n", s2, (int16_t)s2);
    tracef("t2: %f (%04x)\n", t2, (int16_t)t2);

    tracef("invw1: %f (%08lx)\n", invw1, (int32_t)(invw1*65536));
    tracef("invw2: %f (%08lx)\n", invw2, (int32_t)(invw2*65536));
    tracef("invw3: %f (%08lx)\n", invw3, (int32_t)(invw3*65536));
    tracef("minw: %f (%08lx)\n", minw, (int32_t)(minw*65536));

    invw1 *= minw;
    invw2 *= minw;
    invw3 *= minw;

    s1 *= invw1;
    t1 *= invw1;
    s2 *= invw2;
    t2 *= invw2;
    s3 *= invw3;
    t3 *= invw3;

    invw1 *= 0x7FFF;
    invw2 *= 0x7FFF;
    invw3 *= 0x7FFF;

    const float ms = s2 - s1;
    const float mt = t2 - t1;
    const float mw = invw2 - invw1;
    const float hs = s3 - s1;
    const float ht = t3 - t1;
    const float hw = invw3 - invw1;

    const float nxS = data->hy*ms - data->my*hs;
    const float nxT = data->hy*mt - data->my*ht;
    const float nxW = data->hy*mw - data->my*hw;
    const float nyS = data->mx*hs - data->hx*ms;
    const float nyT = data->mx*ht - data->hx*mt;
    const float nyW = data->mx*hw - data->hx*mw;

    const float DsDx = nxS * data->attr_factor;
    const float DtDx = nxT * data->attr_factor;
    const float DwDx = nxW * data->attr_factor;
    const float DsDy = nyS * data->attr_factor;
    const float DtDy = nyT * data->attr_factor;
    const float DwDy = nyW * data->attr_factor;

    const float DsDe = DsDy + DsDx * data->ish;
    const float DtDe = DtDy + DtDx * data->ish;
    const float DwDe = DwDy + DwDx * data->ish;

    const int32_t final_s = float_to_s16_16(s1 + data->fy * DsDe);
    const int32_t final_t = float_to_s16_16(t1 + data->fy * DtDe);
    const int32_t final_w = float_to_s16_16(invw1 + data->fy * DwDe);

    const int32_t DsDx_fixed = float_to_s16_16(DsDx);
    const int32_t DtDx_fixed = float_to_s16_16(DtDx);
    const int32_t DwDx_fixed = float_to_s16_16(DwDx);

    const int32_t DsDe_fixed = float_to_s16_16(DsDe);
    const int32_t DtDe_fixed = float_to_s16_16(DtDe);
    const int32_t DwDe_fixed = float_to_s16_16(DwDe);

    const int32_t DsDy_fixed = float_to_s16_16(DsDy);
    const int32_t DtDy_fixed = float_to_s16_16(DtDy);
    const int32_t DwDy_fixed = float_to_s16_16(DwDy);

    rspq_write_arg(w, (final_s&0xffff0000) | (0xffff&(final_t>>16)));  
    rspq_write_arg(w, (final_w&0xffff0000));
    rspq_write_arg(w, (DsDx_fixed&0xffff0000) | (0xffff&(DtDx_fixed>>16)));
    rspq_write_arg(w, (DwDx_fixed&0xffff0000));    
    rspq_write_arg(w, (final_s<<16) | (final_t&0xffff));
    rspq_write_arg(w, (final_w<<16));
    rspq_write_arg(w, (DsDx_fixed<<16) | (DtDx_fixed&0xffff));
    rspq_write_arg(w, (DwDx_fixed<<16));
    rspq_write_arg(w, (DsDe_fixed&0xffff0000) | (0xffff&(DtDe_fixed>>16)));
    rspq_write_arg(w, (DwDe_fixed&0xffff0000));
    rspq_write_arg(w, (DsDy_fixed&0xffff0000) | (0xffff&(DtDy_fixed>>16)));
    rspq_write_arg(w, (DwDy_fixed&0xffff0000));
    rspq_write_arg(w, (DsDe_fixed<<16) | (DtDe_fixed&0xffff));
    rspq_write_arg(w, (DwDe_fixed<<16));
    rspq_write_arg(w, (DsDy_fixed<<16) | (DtDy_fixed&&0xffff));
    rspq_write_arg(w, (DwDy_fixed<<16));

    tracef("invw1-mul: %f (%08lx)\n", invw1, (int32_t)(invw1*65536));
    tracef("invw2-mul: %f (%08lx)\n", invw2, (int32_t)(invw2*65536));
    tracef("invw3-mul: %f (%08lx)\n", invw3, (int32_t)(invw3*65536));

    tracef("s1w: %f (%04x)\n", s1, (int16_t)s1);
    tracef("t1w: %f (%04x)\n", t1, (int16_t)t1);
    tracef("s2w: %f (%04x)\n", s2, (int16_t)s2);
    tracef("t2w: %f (%04x)\n", t2, (int16_t)t2);

    tracef("ms: %f (%04x)\n", ms, (int16_t)(ms));
    tracef("mt: %f (%04x)\n", mt, (int16_t)(mt));
    tracef("hs: %f (%04x)\n", hs, (int16_t)(hs));
    tracef("ht: %f (%04x)\n", ht, (int16_t)(ht));

    tracef("nxS: %f (%04x)\n", nxS, (int16_t)(nxS / 65536.0f));
    tracef("nxT: %f (%04x)\n", nxT, (int16_t)(nxT / 65536.0f));
    tracef("nyS: %f (%04x)\n", nyS, (int16_t)(nyS / 65536.0f));
    tracef("nyT: %f (%04x)\n", nyT, (int16_t)(nyT / 65536.0f));
}

__attribute__((always_inline))
static inline void __rdpq_write_zbuf_coeffs(rspq_write_t *w, rdpq_tri_edge_data_t *data, const float *v1, const float *v2, const float *v3)
{
    const float z1 = v1[0] * 0x7FFF;
    const float z2 = v2[0] * 0x7FFF;
    const float z3 = v3[0] * 0x7FFF;

    const float mz = z2 - z1;
    const float hz = z3 - z1;

    const float nxz = data->hy*mz - data->my*hz;
    const float nyz = data->mx*hz - data->hx*mz;

    const float DzDx = nxz * data->attr_factor;
    const float DzDy = nyz * data->attr_factor;
    const float DzDe = DzDy + DzDx * data->ish;

    const int32_t final_z = float_to_s16_16(z1 + data->fy * DzDe);
    const int32_t DzDx_fixed = float_to_s16_16(DzDx);
    const int32_t DzDe_fixed = float_to_s16_16(DzDe);
    const int32_t DzDy_fixed = float_to_s16_16(DzDy);

    rspq_write_arg(w, final_z);
    rspq_write_arg(w, DzDx_fixed);
    rspq_write_arg(w, DzDe_fixed);
    rspq_write_arg(w, DzDy_fixed);

    tracef("z1: %f (%04x)\n", v1[0], (uint16_t)(z1));
    tracef("z2: %f (%04x)\n", v2[0], (uint16_t)(z2));
    tracef("z3: %f (%04x)\n", v3[0], (uint16_t)(z3));

    tracef("mz: %f (%04x)\n", mz, (uint16_t)(mz));
    tracef("hz: %f (%04x)\n", hz, (uint16_t)(hz));

    tracef("nxz: %f (%08lx)\n", nxz, (uint32_t)(nxz * 4.0f));
    tracef("nyz: %f (%08lx)\n", nyz, (uint32_t)(nyz * 4.0f));

    tracef("dzdx: %f (%08llx)\n", DzDx, (uint64_t)(DzDx * 65536.0f));
    tracef("dzdy: %f (%08llx)\n", DzDy, (uint64_t)(DzDy * 65536.0f));
    tracef("dzde: %f (%08llx)\n", DzDe, (uint64_t)(DzDe * 65536.0f));
}

/** @brief RDP triangle primitive assembled on the CPU */
void rdpq_triangle_cpu(const rdpq_trifmt_t *fmt, const float *v1, const float *v2, const float *v3)
{
    uint32_t res = AUTOSYNC_PIPE;
    if (fmt->tex_offset >= 0) {
        // FIXME: this can be using multiple tiles depending on color combiner and texture
        // effects such as detail and sharpen. Figure it out a way to handle these in the
        // autosync engine.
        res |= AUTOSYNC_TILE(fmt->tex_tile);
        res |= AUTOSYNC_TMEMS;
    }
    __rdpq_autosync_use(res);

    uint32_t cmd_id = RDPQ_CMD_TRI;

    uint32_t size = 8;
    if (fmt->shade_offset >= 0) {
        size += 16;
        cmd_id |= 0x4;
    }
    if (fmt->tex_offset >= 0) {
        size += 16;
        cmd_id |= 0x2;
    }
    if (fmt->z_offset >= 0) {
        size += 4;
        cmd_id |= 0x1;
    }

    rspq_write_t w = rspq_write_begin(RDPQ_OVL_ID, cmd_id, size);

    if( v1[fmt->pos_offset + 1] > v2[fmt->pos_offset + 1] ) { SWAP(v1, v2); }
    if( v2[fmt->pos_offset + 1] > v3[fmt->pos_offset + 1] ) { SWAP(v2, v3); }
    if( v1[fmt->pos_offset + 1] > v2[fmt->pos_offset + 1] ) { SWAP(v1, v2); }

    rdpq_tri_edge_data_t data;
    __rdpq_write_edge_coeffs(&w, &data, fmt->tex_tile, fmt->tex_mipmaps, v1 + fmt->pos_offset, v2 + fmt->pos_offset, v3 + fmt->pos_offset);

    if (fmt->shade_offset >= 0) {
        const float *shade_v2 = fmt->shade_flat ? v1 : v2;
        const float *shade_v3 = fmt->shade_flat ? v1 : v3;
        __rdpq_write_shade_coeffs(&w, &data, v1 + fmt->shade_offset, shade_v2 + fmt->shade_offset, shade_v3 + fmt->shade_offset);
    }

    if (fmt->tex_offset >= 0) {
        __rdpq_write_tex_coeffs(&w, &data, v1 + fmt->tex_offset, v2 + fmt->tex_offset, v3 + fmt->tex_offset);
    }

    if (fmt->z_offset >= 0) {
        __rdpq_write_zbuf_coeffs(&w, &data, v1 + fmt->z_offset, v2 + fmt->z_offset, v3 + fmt->z_offset);
    }

    rspq_write_end(&w);
}

/** @brief RDP triangle primitive assembled on the RSP */
void rdpq_triangle_rsp(const rdpq_trifmt_t *fmt, const float *v1, const float *v2, const float *v3)
{
    uint32_t res = AUTOSYNC_PIPE;
    if (fmt->tex_offset >= 0) {
        // FIXME: this can be using multiple tiles depending on color combiner and texture
        // effects such as detail and sharpen. Figure it out a way to handle these in the
        // autosync engine.
        res |= AUTOSYNC_TILE(fmt->tex_tile);
        res |= AUTOSYNC_TMEM(0);
    }
    __rdpq_autosync_use(res);

    uint32_t cmd_id = RDPQ_CMD_TRI;
    if (fmt->shade_offset >= 0) cmd_id |= 0x4;
    if (fmt->tex_offset >= 0)   cmd_id |= 0x2;
    if (fmt->z_offset >= 0)     cmd_id |= 0x1;

    const int TRI_DATA_LEN = ROUND_UP((2+1+1+3)*4, 16);

    const float *vtx[3] = {v1, v2, v3};
    for (int i=0;i<3;i++) {
        const float *v = vtx[i];

        // X,Y: s13.2
        int16_t x = floorf(v[fmt->pos_offset+0] * 4.0f);
        int16_t y = floorf(v[fmt->pos_offset+1] * 4.0f);
        
        int16_t z = 0;
        if (fmt->z_offset >= 0) {
            z = v[fmt->z_offset+0] * 0x7FFF;
        } 

        int32_t rgba = 0;
        if (fmt->shade_offset >= 0) {
            const float *v_shade = fmt->shade_flat ? v1 : v;
            uint32_t r = v_shade[fmt->shade_offset+0] * 255.0;
            uint32_t g = v_shade[fmt->shade_offset+1] * 255.0;
            uint32_t b = v_shade[fmt->shade_offset+2] * 255.0;
            uint32_t a = v_shade[fmt->shade_offset+3] * 255.0;
            rgba = (r << 24) | (g << 16) | (b << 8) | a;
        }

        int16_t s=0, t=0;
        int32_t w=0, inv_w=0;
        if (fmt->tex_offset >= 0) {
            s     = v[fmt->tex_offset+0] * 32.0f;
            t     = v[fmt->tex_offset+1] * 32.0f;
            w     = float_to_s16_16(1.0f / v[fmt->tex_offset+2]);
            inv_w = float_to_s16_16(       v[fmt->tex_offset+2]);
        }

        rspq_write(RDPQ_OVL_ID, RDPQ_CMD_TRIANGLE_DATA,
            TRI_DATA_LEN * i, 
            (x << 16) | (y & 0xFFFF), 
            (z << 16), 
            rgba, 
            (s << 16) | (t & 0xFFFF), 
            w,
            inv_w);
    }

    rspq_write(RDPQ_OVL_ID, RDPQ_CMD_TRIANGLE, 
        0xC000 | (cmd_id << 8) | 
        (fmt->tex_mipmaps ? (fmt->tex_mipmaps-1) << 3 : 0) | 
        (fmt->tex_tile & 7));
}

void rdpq_triangle(const rdpq_trifmt_t *fmt, const float *v1, const float *v2, const float *v3)
{
#if RDPQ_TRIANGLE_REFERENCE
    rdpq_triangle_cpu(fmt, v1, v2, v3);
#else
    rdpq_triangle_rsp(fmt, v1, v2, v3);
#endif
}
