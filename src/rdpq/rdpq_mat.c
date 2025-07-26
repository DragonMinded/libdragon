/**
 * @file rdpq_mat.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief RDP Command queue: material system
 * @ingroup rdpq
 */
///@cond
#define _GNU_SOURCE
///@endcond
#include <stdint.h>
#include <stdlib.h>
#include "asset.h"
#include "sprite.h"
#include "rdpq_mat.h"
#include "rdpq_tex.h"
#include "rdpq_debug.h"
#include "rdpq_mode.h"
#include "rdpq_sprite.h"
#include "rdpq_mat_internal.h"
#include "../hashtable_internal.h"

/** Fetch a value from the binary packed material */
#define FETCH(data, type) ({ \
    typedef type unaligned_type __attribute__((aligned(1))); \
    unaligned_type* __value = (unaligned_type*)data; \
    data += sizeof(type); \
    *__value; \
})

/** Decode a binary packed pointer offset */
#define PTR_DECODE(font, ptr)    ((void*)(((uint8_t*)(font)) + (uint32_t)(ptr)))

/** Global texture cache */
static hashtable_t tex_cache;
static char *texdb_path;

static void* tex_cache_loader(uint32_t key)
{
    assertf(texdb_path, "texture DB path not configured; call rdpq_mat_set_texture_path() first");

    // Convert the key to a hex string within the texdb_path
    int pos = strlen(texdb_path) - 8;
    for (int i=0; i<8; i++) {
        char hexdigit = "0123456789abcdef"[key & 0xF];
        key >>= 4;
        texdb_path[pos - i] = hexdigit;
    }

    return sprite_load(texdb_path);
}

void rdpq_mat_set_texture_path(const char *path)
{
    free(texdb_path);
    if (path == NULL) {
        texdb_path = NULL;
    } else {
        asprintf(&texdb_path, "%s/00000000.sprite", path);
    }
}

static void mat_load(void *mat)
{
    mat += FETCH(mat, uint8_t); // skip the name
    uint16_t flags = FETCH(mat, uint16_t);

    if (flags & MATFLAG_TEXTURE) {
        uint32_t tex0 = FETCH(mat, uint32_t);
        uint32_t tex1 = FETCH(mat, uint32_t);

        if (tex0) hashtable_insert(&tex_cache, tex0, NULL);
        if (tex1) hashtable_insert(&tex_cache, tex1, NULL);
    }
}

static void mat_unload(void *mat)
{
    mat += FETCH(mat, uint8_t); // skip the name
    uint16_t flags = FETCH(mat, uint16_t);

    if (flags & MATFLAG_TEXTURE) {
        uint32_t tex0 = FETCH(mat, uint32_t);
        uint32_t tex1 = FETCH(mat, uint32_t);

        if (tex0) {
            sprite_t *tex = hashtable_remove(&tex_cache, tex0);
            if (tex) sprite_free(tex);
        }
        if (tex1) {
            sprite_t *tex = hashtable_remove(&tex_cache, tex1);
            if (tex) sprite_free(tex);
        }
    }
}


rdpq_matdb_t* rdpq_matdb_open(const char *filename)
{
    rdpq_matdb_t *mdb = asset_load(filename, NULL);
    assertf(memcmp(mdb->id, "MDB", 3) == 0, "Invalid MDB header: %s", filename);
    assertf(mdb->version == 1, "Invalid MDB version (%d): %s", mdb->version, filename);
    return mdb;
}

void rdpq_matdb_close(rdpq_matdb_t *mdb)
{
    free(mdb);
}

rdpq_mat_t* rdpq_matdb_load(rdpq_matdb_t *mdb, const char *mat_name)
{
    uint8_t *index = mdb->index;
    void *ptr = (void*)mdb + sizeof(rdpq_matdb_t) + mdb->num_materials * sizeof(uint8_t) * 2;

    if (!mat_name) {
        mat_load(ptr);
        return ptr;
    }

    int mat_name_len = strlen(mat_name);
    for (int i = 0; i < mdb->num_materials; i++) {
        uint8_t namelen = *index++;
        uint8_t size = *index++;

        if (namelen == mat_name_len) {
            assertf(*(uint8_t*)ptr == namelen, "Invalid material name length in MDB");
            if (memcmp(ptr+1, mat_name, namelen) == 0) {
                mat_load(ptr);
                return ptr;
            }
        }

        ptr += size;
    }

    return NULL;
}

rdpq_mat_t* rdpq_mat_load_buf(void *buf, int size)
{
    rdpq_mat_t *ptr = buf;
    mat_load(ptr);
    return ptr;
}

void rdpq_mat_draw_begin(rdpq_mat_t *mat_ptr)
{
    void *mat = mat_ptr;
    int namelen = FETCH(mat, uint8_t);
    char *name = (char*)mat;
    mat += namelen;

    uint16_t flags = FETCH(mat, uint16_t);
    bool has_overrides = flags & MATFLAG_RMO_MASK;
    if (has_overrides) {
        rdpq_mode_push();
    }

    rdpq_mode_begin();

    if (flags & MATFLAG_TEXTURE) {
        int tex0 = FETCH(mat, uint32_t);
        int tex1 = FETCH(mat, uint32_t);
        if (tex0) {
            if (tex1) rdpq_tex_multi_begin(); 
            sprite_t *tex0_sprite = hashtable_lookup(&tex_cache, tex0);
            rdpq_sprite_upload(TILE0, tex0_sprite, NULL);
            if (tex1) {
                rdpq_sprite_upload(TILE1, hashtable_lookup(&tex_cache, tex1), NULL);
                rdpq_tex_multi_end();
            }
        }
    }
    if (flags & MATFLAG_COMBINER) {
        rdpq_combiner_t cc = FETCH(mat, uint64_t);
        rdpq_mode_combiner(cc);
    }
    if (flags & MATFLAG_BLENDER) {
        rdpq_blender_t bl = FETCH(mat, uint32_t);
        rdpq_mode_blender(bl);
    }
    if (flags & MATFLAG_RMO_AA) {
        rdpq_antialias_t aa = FETCH(mat, uint8_t);
        rdpq_mode_antialias(aa);
    }
    if (flags & MATFLAG_RMO_FOG) {
        rdpq_blender_t fog = FETCH(mat, uint32_t);
        rdpq_mode_fog(fog);   
    }
    if (flags & MATFLAG_RMO_DITHERING) {
        rdpq_dither_t dither = FETCH(mat, uint8_t);
        rdpq_mode_dithering(dither);
    }
    if (flags & MATFLAG_RMO_FILTERING) {
        rdpq_filter_t filter = FETCH(mat, uint8_t);
        rdpq_mode_filter(filter);
    }
    if (flags & MATFLAG_RMO_ZMODE) {
        uint8_t zmode = FETCH(mat, uint8_t);
        rdpq_mode_zbuf(zmode & 1, zmode & 2);
    }
    if (flags & MATFLAG_RMO_ZPRIM) {
        int16_t z_prim = FETCH(mat, int16_t);
        int16_t deltaz_prim = FETCH(mat, int16_t);
        rdpq_mode_zoverride(true, z_prim, deltaz_prim);
    }
    if (flags & MATFLAG_RMO_PERSP) {
        uint8_t persp = FETCH(mat, uint8_t);
        rdpq_mode_persp(persp);
    }
    if (flags & MATFLAG_RMO_ACMP) {
        uint8_t acmp = FETCH(mat, uint8_t);
        rdpq_mode_alphacompare(acmp);
    }
    if (flags & MATFLAG_UNIFORM_K4K5) {
        uint16_t k4k5 = FETCH(mat, uint16_t);
        rdpq_set_yuv_parms(0, 0, 0, 0, k4k5 >> 8, k4k5 & 0xFF);
    }
    if (flags & MATFLAG_UNIFORM_CHROMAKEY) {
        (void)FETCH(mat, uint16_t);
        assertf(0, "MATFLAG_UNIFORM_CHROMAKEY not implemented");
    }
    if (flags & MATFLAG_UNIFORM_PRIMLODFRAC) {
        uint8_t prim_lod_frac = FETCH(mat, uint8_t);
        rdpq_set_prim_lod_frac(prim_lod_frac);
    }
    if (flags & MATFLAG_UNIFORM_PRIM) {
        uint32_t prim = FETCH(mat, uint32_t);
        rdpq_set_prim_color(color_from_packed32(prim));
    }
    if (flags & MATFLAG_UNIFORM_ENV) {
        uint32_t env = FETCH(mat, uint32_t);
        rdpq_set_env_color(color_from_packed32(env));
    }

    assertf(FETCH(mat, uint8_t) == 0xAB, "material %.*s: missing terminator", namelen, name);
    rdpq_mode_end();
}

void rdpq_mat_draw_end(rdpq_mat_t *mat_ptr)
{
    void *mat = mat_ptr;
    mat += FETCH(mat, uint8_t); // skip the name
    uint16_t flags = FETCH(mat, uint16_t);
    bool has_overrides = flags & MATFLAG_RMO_MASK;

    if (has_overrides)
        rdpq_mode_pop();
}

void rdpq_mat_free(rdpq_mat_t *mat)
{
    mat_unload(mat);
}

/** Constructor to initialize the texture cache */
__attribute__((constructor))
void __rdpq_mat_init(void)
{
    // Initialize the texture cache
    hashtable_init(&tex_cache, 32, tex_cache_loader);
}
