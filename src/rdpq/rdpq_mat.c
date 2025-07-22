/**
 * @file rdpq_mat.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "n64sys.h"
#include "debug.h"
#include "sprite.h"
#include "asset_internal.h"
#include "rspq.h"
#include "../rspq/rspq_internal.h"
#include "rdpq_debug.h"
#include "rdpq_mode.h"
#include "rdpq_sprite.h"
#include "rdpq_mat.h"
#include "rdpq_mat_internal.h"

#define FETCH(data, type) ({ \
    typedef type unaligned_type __attribute__((aligned(1))); \
    unaligned_type* __value = (unaligned_type*)data; \
    data += sizeof(type); \
    *__value; \
})

#define PTR_DECODE(font, ptr)    ((void*)(((uint8_t*)(font)) + (uint32_t)(ptr)))

static void mat_load_texture(rdpq_matdb_t *mdb, uint16_t texid)
{
    if (mdb->texcache[texid].refcount > 0) {
        mdb->texcache[texid].refcount++;
        return;
    }

    rdpq_matdb_textable_t tt = mdb->head->textures[texid];

    // Seek to the texture data in the file and load the sprite.
    // Go through the asset system to handle decompression
    lseek(mdb->fd, tt.offset, SEEK_SET);
    void *buf = asset_loadfd(mdb->fd, &tt.size);
    sprite_t *sprite = sprite_load_buf(buf, tt.size);

    // Cache the sprite in the texture cache
    mdb->texcache[texid].sprite_ptr = PhysicalAddr(sprite);
    mdb->texcache[texid].refcount = 1;
}

static void mat_unload_texture(rdpq_matdb_t *mdb, uint16_t texid)
{
    if (--mdb->texcache[texid].refcount > 0) {
        return;
    }

    // Free the sprite. Since we loaded it with sprite_load_buf,
    // we must free the buffer ourselves.
    sprite_t *sprite = (sprite_t*)VirtualCachedAddr(mdb->texcache[texid].sprite_ptr);
    sprite_free(sprite);
    free(sprite);
}

static uint32_t prime_hash(const char *str, uint32_t prime)
{
    uint32_t hash = 0;
    while(*str) {
        char c = *str++;
        hash = (hash * prime) + c;
    }
    return hash;
}

static int compare_hashes(const void *a, const void *b)
{
    uint32_t ha = *(uint32_t*)a;
    uint32_t hb = ((rdpq_matdb_hashtable_t*)b)->hash;
    return ha - hb;
}

static void* mat_lookup(rdpq_matdb_t *mdb, const char *name)
{
    uint32_t hash = prime_hash(name, mdb->head->hash_prime);
    rdpq_matdb_hashtable_t *entry = bsearch(&hash, mdb->head->ht, mdb->head->num_materials, sizeof(rdpq_matdb_hashtable_t), compare_hashes);
    assertf(entry, "material %s not found in database", name);
    void *data = entry->data;

    uint8_t namelen = FETCH(data, uint8_t);
    assertf(namelen == strlen(name) && memcmp(data, name, namelen) == 0,
        "material %s not found in database", name);
    data += namelen;

    return data;
}

static void mat_begin(rdpq_matdb_t *mdb, const char *name, void *data)
{
    uint16_t flags = FETCH(data, uint16_t);
    bool has_overrides = flags & MATFLAG_RMO_MASK;
    if (has_overrides) {
        rdpq_mode_push();
    }

    rdpq_mode_begin();

    if (flags & MATFLAG_TEXTURE) {
        int16_t texid = FETCH(data, int16_t);
        assertf(texid >= 0 && texid < mdb->head->num_textures, "invalid texture ID %d (corrupted matdb?)", texid);
        assertf(mdb->texcache[texid].refcount >= 0, "material %d not loaded", texid);
        sprite_t *sprite = (sprite_t*)VirtualCachedAddr(mdb->texcache[texid].sprite_ptr);
        rdpq_sprite_upload(TILE0, sprite, NULL);
    }
    if (flags & MATFLAG_COMBINER) {
        rdpq_combiner_t cc = FETCH(data, uint64_t);
        rdpq_mode_combiner(cc);
    }
    if (flags & MATFLAG_BLENDER) {
        rdpq_blender_t bl = FETCH(data, uint32_t);
        rdpq_mode_blender(bl);
    }
    if (flags & MATFLAG_RMO_AA) {
        rdpq_antialias_t aa = FETCH(data, uint8_t);
        rdpq_mode_antialias(aa);
    }
    if (flags & MATFLAG_RMO_FOG) {
        rdpq_blender_t fog = FETCH(data, uint32_t);
        rdpq_mode_fog(fog);   
    }
    if (flags & MATFLAG_RMO_DITHERING) {
        rdpq_dither_t dither = FETCH(data, uint8_t);
        rdpq_mode_dithering(dither);
    }
    if (flags & MATFLAG_RMO_FILTERING) {
        rdpq_filter_t filter = FETCH(data, uint8_t);
        rdpq_mode_filter(filter);
    }
    if (flags & MATFLAG_RMO_ZMODE) {
        uint8_t zmode = FETCH(data, uint8_t);
        rdpq_mode_zbuf(zmode & 1, zmode & 2);
    }
    if (flags & MATFLAG_RMO_ZPRIM) {
        int16_t z_prim = FETCH(data, int16_t);
        int16_t deltaz_prim = FETCH(data, int16_t);
        rdpq_mode_zoverride(true, z_prim, deltaz_prim);
    }
    if (flags & MATFLAG_RMO_PERSP) {
        uint8_t persp = FETCH(data, uint8_t);
        rdpq_mode_persp(persp);
    }
    if (flags & MATFLAG_RMO_ACMP) {
        uint8_t acmp = FETCH(data, uint8_t);
        rdpq_mode_alphacompare(acmp);
    }
    if (flags & MATFLAG_UNIFORM_K4K5) {
        uint16_t k4k5 = FETCH(data, uint16_t);
        rdpq_set_yuv_parms(0, 0, 0, 0, k4k5 >> 8, k4k5 & 0xFF);
    }
    if (flags & MATFLAG_UNIFORM_CHROMAKEY) {
        (void)FETCH(data, uint16_t);
        assertf(0, "MATFLAG_UNIFORM_CHROMAKEY not implemented");
    }
    if (flags & MATFLAG_UNIFORM_PRIMLODFRAC) {
        uint8_t prim_lod_frac = FETCH(data, uint8_t);
        rdpq_set_prim_lod_frac(prim_lod_frac);
    }
    if (flags & MATFLAG_UNIFORM_PRIM) {
        uint32_t prim = FETCH(data, uint32_t);
        rdpq_set_prim_color(color_from_packed32(prim));
    }
    if (flags & MATFLAG_UNIFORM_ENV) {
        uint32_t env = FETCH(data, uint32_t);
        rdpq_set_env_color(color_from_packed32(env));
    }

    assertf(FETCH(data, uint8_t) == 0xAB, "material %s: missing terminator", name);
    rdpq_mode_end();
}

static void mat_end(rdpq_matdb_t *mdb, const char *mat_name, void *data)
{
    uint16_t flags = FETCH(data, uint16_t);
    bool has_overrides = flags & MATFLAG_RMO_MASK;

    if (has_overrides)
        rdpq_mode_pop();
}

void mat_load(rdpq_matdb_t *mdb, const char *mat_name, void *data)
{
    uint16_t flags = FETCH(data, uint16_t);

    if (flags & MATFLAG_TEXTURE) {
        int16_t texid = FETCH(data, int16_t);
        assertf(texid >= 0 && texid < mdb->head->num_textures, "invalid texture ID %d (corrupted matdb?)", texid);
        mat_load_texture(mdb, texid);
    }
}

void mat_unload(rdpq_matdb_t *mdb, const char *mat_name, void *data)
{
    uint16_t flags = FETCH(data, uint16_t);

    if (flags & MATFLAG_TEXTURE) {
        int16_t texid = FETCH(data, int16_t);
        assertf(texid >= 0 && texid < mdb->head->num_textures, "invalid texture ID %d (corrupted matdb?)", texid);
        assertf(mdb->texcache[texid].refcount > 0, "material %s not preloaded", mat_name);
        mat_unload_texture(mdb, texid);
    }
}

rdpq_matdb_t *rdpq_matdb_open(const char *filename, bool autoload)
{
    rdpq_matdb_header_t head = {0};

    int fd = must_open(filename);

    read(fd, &head, sizeof(head));
    assertf(memcmp(head.id, "MDB", 3) == 0, "Invalid MDB header");
    assertf(head.version == 1, "Invalid MDB version: %d", head.version);

    // Allocate memory for the whole database
    int texcache_size = head.num_textures * sizeof(rdpq_matdb_texcache_t);
    rdpq_matdb_t *mdb = malloc(sizeof(rdpq_matdb_t) + head.meta_size + texcache_size);
    mdb->head = (void*)mdb + sizeof(rdpq_matdb_t) + texcache_size;
    mdb->fd = fd;

    // Read again the whole header, plus the metadata
    lseek(fd, 0, SEEK_SET);
    read(fd, mdb->head, head.meta_size);

    // Decode internal pointers
    mdb->head->textures = PTR_DECODE(mdb->head, mdb->head->textures);
    for (int i=0; i<mdb->head->num_materials; i++)
        mdb->head->ht[i].data = PTR_DECODE(mdb->head, mdb->head->ht[i].data);

    // Autoload textures if requested
    if (autoload) {
        for (int i=0; i<head.num_textures; i++) {
            mat_load_texture(mdb, i);
        }
        mdb->head->flags |= MATDBFLAG_AUTOLOAD;
    }

    return mdb;
}

void rdpq_matdb_close(rdpq_matdb_t *mdb)
{
    if (mdb->head->flags & MATDBFLAG_AUTOLOAD) {
        for (int i=0; i<mdb->head->num_textures; i++) {
            mat_unload_texture(mdb, i);
        }
    }

    for (int i=0; i<mdb->head->num_textures; i++) {
        assertf(mdb->texcache[i].refcount == 0, "texture %d still in use in material DB (rdpq_matdb_unload not called)", i);
    }
    close(mdb->fd);
    free(mdb);
}

void rdpq_matdb_load(rdpq_matdb_t* mdb, const char *mat_name)
{
    mat_load(mdb, mat_name, mat_lookup(mdb, mat_name));
}

void rdpq_matdb_unload(rdpq_matdb_t* mdb, const char *mat_name)
{
    mat_unload(mdb, mat_name, mat_lookup(mdb, mat_name));
}

void rdpq_matdb_begin(rdpq_matdb_t* mdb, const char *mat_name)
{
    mat_begin(mdb, mat_name, mat_lookup(mdb, mat_name));
}

void rdpq_matdb_end(rdpq_matdb_t* mdb, const char *mat_name)
{
    mat_end(mdb, mat_name, mat_lookup(mdb, mat_name));
}

void rdpq_matdb_debug_dump(rdpq_matdb_t* mdb, const char *mat_name)
{
    rspq_wait();

    // Go through all materials and print their names
    for (int i=0; i<mdb->head->num_materials; i++) {
        void *data = mdb->head->ht[i].data;
        uint8_t namelen = FETCH(data, uint8_t);
        char name[namelen+1];
        memcpy(name, data, namelen);
        name[namelen] = 0;
        data += namelen;

        if (!mat_name || !strcmp(name, mat_name)) {
            debugf("MDB DEBUG: material %d: %s\n", i, name);

            rdpq_debug_log(true);
            mat_load(mdb, name, data);
            mat_begin(mdb, name, data);
            rdpq_debug_log(false);

            uint64_t som = rdpq_get_other_modes_raw();

            mat_end(mdb, name, data);
            mat_unload(mdb, name, data);
            rspq_wait();
            debugf("MDB DEBUG: SOM: %016llx\n", som);
        }
    }
}
