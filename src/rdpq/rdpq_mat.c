/**
 * @file rdpq_mat.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief RDP Command queue: material system
 * @ingroup rdpq
 * 
 * ## Material database binary format
 * 
 * A material database (.mdb file) contains multiple materials packed together
 * for efficient loading and storage. The binary format is as follows:
 * 
 * Header:
 * - 3 bytes: ID string "MDB"
 * - 1 byte: Version number (currently 1)
 * - 2 bytes: Number of materials in the database (big-endian)
 * - 2 bytes: Flags (reserved for future use)
 * 
 * Index Section:
 * After the header follows an index section with one entry per material:
 * - 1 byte: Name length of the material
 * - 1 byte: Total size of the material data (including name)
 * 
 * After the index section, the actual material data follows. Each material
 * is stored consecutively using the "Material binary format" described below.
 * The materials are stored in the same order as their index entries.
 * 
 * The #rdpq_matdb_load function uses the index to quickly locate a specific
 * material by name without having to parse all materials sequentially.
 * 
 * ## Material binary format
 * 
 * A single material is stored in a packed binary format as follows.
 * 
 * - The first byte is the name length (up to 255 characters).
 * - The next bytes are the name of the material (without null terminator).
 * - The next 2 bytes are the flags, which can be a combination of the various
 *   MATFLAG_* flags (see rdpq_mat_internal.h). Each flag is a bit that specifies
 *   whether a certain feature is present in the material.
 * - The next byte is the offset of the extension data (since this position in the
 *   material), which is a variable-length array of extension keys and values.
 *   See below for a description of the extension system.
 * - Then follows the various features of the material, depending on the flags.
 *   The features are stored in the following order:
 *   - If MATFLAG_TEXTURE is set, the next 8 bytes are the hashes of two textures
 *     (tex0 and tex1) that can be found in the texture database.
 *   - If MATFLAG_COMBINER is set, the next 8 bytes are the combiner formula to
 *     pass to #rdpq_mode_combiner.
 *   - If MATFLAG_BLENDER is set, the next 4 bytes are the blender formula to
 *     pass to #rdpq_mode_blender.
 *   - If MATFLAG_RMO_AA is set, the next 1 byte is the antialiasing mode to
 *     pass to #rdpq_mode_antialias.
 *   - If MATFLAG_RMO_FOG is set, the next 4 bytes are the fog blender formula to
 *     pass to #rdpq_mode_fog.
 *   - If MATFLAG_RMO_DITHERING is set, the next 1 byte is the dithering mode to
 *     pass to #rdpq_mode_dithering.
 *   - If MATFLAG_RMO_FILTERING is set, the next 1 byte is the filtering mode to
 *     pass to #rdpq_mode_filter.
 *   - If MATFLAG_RMO_ZMODE is set, the next 1 byte contains the Z-buffer mode
 *     (bit 0: enable Z-buffer, bit 1: enable Z-compare) to pass to #rdpq_mode_zbuf.
 *   - If MATFLAG_RMO_ZPRIM is set, the next 4 bytes are two 16-bit values
 *     (z_prim and deltaz_prim) to pass to #rdpq_mode_zoverride.
 *   - If MATFLAG_RMO_PERSP is set, the next 1 byte is the perspective correction
 *     mode to pass to #rdpq_mode_persp.
 *   - If MATFLAG_RMO_ACMP is set, the next 1 byte is the alpha compare mode to
 *     pass to #rdpq_mode_alphacompare.
 *   - If MATFLAG_UNIFORM_K4K5 is set, the next 2 bytes are the K4 and K5 YUV
 *     conversion parameters (K4 in upper byte, K5 in lower byte).
 *   - If MATFLAG_UNIFORM_CHROMAKEY is set, the next 2 bytes are the chroma key
 *     parameters (currently not implemented).
 *   - If MATFLAG_UNIFORM_PRIMLODFRAC is set, the next 1 byte is the primitive
 *     LOD fraction to pass to #rdpq_set_prim_lod_frac.
 *   - If MATFLAG_UNIFORM_PRIM is set, the next 4 bytes are the packed primitive
 *     color to pass to #rdpq_set_prim_color.
 *   - If MATFLAG_UNIFORM_ENV is set, the next 4 bytes are the packed environment
 *     color to pass to #rdpq_set_env_color.
 * - Finally, the material ends with a terminator byte (0xAB).
 * 
 * ## Ext binary format
 * 
 * This is the binary format used to store optional extension keys and values,
 * after the terminator byte (0xAB) of the material. The offset to this section
 * can be found in the material (see above).
 *
 * First byte in the extension data is the header, which contains the number
 * of extension keys (up to 63).
 *
 * Then there is the key array. It's a sequence of 2-byte keys (big-endian), which represents
 * the lower 16 bits of the hash of the extension key. Keys are sorted in ascending order.
 *
 * Then there is the value array. It's a sequence of 2-byte metadata entries, one per extension key.
 * Each metadata entry contains: the first byte is the flags containing the type (see EXT_FLAG_*), 
 * and the second byte is the offset of the actual value content from the current position 
 * (after the value array).
 *
 * Then the actual values follow, one after the other. Values are stored as follows:
 * - int: 4 bytes, big-endian
 * - float: 4 bytes, big-endian
 * - string: null-terminated string
 * - bool: as a special case, the bool value is stored as a single bit (EXT_FLAG_BOOLBIT) 
 *   in the flags byte, no separate value storage needed.
 * 
 * 
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
#include "../utils.h"

/** Fetch a value from the binary packed material */
#define FETCH(data, type) ({ \
    typedef type unaligned_type __attribute__((aligned(1))); \
    unaligned_type* __value = (unaligned_type*)data; \
    data += sizeof(type); \
    *__value; \
})

/** Decode a binary packed pointer offset */
#define PTR_DECODE(font, ptr)    ((void*)(((uint8_t*)(font)) + (uint32_t)(ptr)))

// Internal flags used for the material extension system
#define EXT_FLAG_TYPE    0x07       ///< Type of the extension value (mask)
#define EXT_FLAG_BOOLBIT 0x08       ///< Boolean value (in case the type is bool)

// Types of extension values
#define EXT_TYPE_BOOL    0x00       ///< Extension type: boolean
#define EXT_TYPE_INT     0x01       ///< Extension type: integer
#define EXT_TYPE_STRING  0x02       ///< Extension type: string
#define EXT_TYPE_FLOAT   0x03       ///< Extension type: float


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
    mat++; // skip extension offset

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
    mat++; // skip extension offset

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

static void mat_get_tex(void *mat, sprite_t **tex0, sprite_t **tex1)
{
    mat += FETCH(mat, uint8_t); // skip the name
    uint16_t flags = FETCH(mat, uint16_t);
    mat++; // skip extension offset

    if (flags & MATFLAG_TEXTURE) {
        uint32_t t0 = FETCH(mat, uint32_t);
        uint32_t t1 = FETCH(mat, uint32_t);

        if (t0) *tex0 = hashtable_lookup(&tex_cache, t0);
        if (t1) *tex1 = hashtable_lookup(&tex_cache, t1);
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
    bool has_overrides = flags & MATFLAG_SOM_MASK;
    if (has_overrides) {
        rdpq_mode_push();
    }

    // Skip extension offset
    mat++;

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
    bool has_overrides = flags & MATFLAG_SOM_MASK;

    if (has_overrides)
        rdpq_mode_pop();
}

void rdpq_mat_free(rdpq_mat_t *mat)
{
    mat_unload(mat);
}

static bool mat_ext_lookup(uint8_t **mat_ext, uint32_t ext_key, uint8_t *ext_flags)
{
    uint8_t head = *(*mat_ext)++;
    uint8_t *keys = *mat_ext;

    int num_exts = head & 63;
    uint8_t *meta = NULL;

    ext_key &= 0xFFFF;
    *mat_ext += num_exts * 2;
    int left = 0, right = num_exts - 1;
    while (left <= right) {
        int mid = (left + right) / 2;
        uint16_t key = (keys[mid * 2] << 8) | keys[mid * 2 + 1];
        if (key == ext_key) {
            meta = *mat_ext + mid * 2;
            break;
        }
        if (key < ext_key) left = mid + 1;
        else right = mid - 1;
    }
    if (!meta) return false;
        
    *mat_ext += num_exts * 2;
    *ext_flags = *meta++;
    *mat_ext += *meta++;
    return true;
}

static bool mat_ext_get(void *mat, uint32_t ext_key, void *ext_value, int ext_type)
{
    // Skip name and flag; now mat points to the extension offset
    uint8_t namelen = FETCH(mat, uint8_t);
    uint8_t *mat_ext = (uint8_t*)mat + namelen + 2;
    if (*mat_ext == 0)
        return false;
    mat_ext += mat_ext[0];

    uint8_t ext_flags;
    if (!mat_ext_lookup(&mat_ext, ext_key, &ext_flags) ||
        (ext_flags & EXT_FLAG_TYPE) != ext_type)
        return false;
    
    switch (ext_type) {
        case EXT_TYPE_BOOL:
            *(bool*)ext_value = (ext_flags & EXT_FLAG_BOOLBIT) != 0;
            return true;
        case EXT_TYPE_INT:
            *(uint32_t*)ext_value = (mat_ext[0] << 24) | (mat_ext[1] << 16) | (mat_ext[2] << 8) | mat_ext[3];
            return true;
        case EXT_TYPE_STRING:
            *(char**)ext_value = (char*)mat_ext;
            return true;
        case EXT_TYPE_FLOAT: {
            uint32_t fi = (mat_ext[0] << 24) | (mat_ext[1] << 16) | (mat_ext[2] << 8) | mat_ext[3];
            *(float*)ext_value = I2F(fi);
        }   return true;
        default:
            return false; // unsupported type
    }
}

void rdpq_mat_get_textures(rdpq_mat_t *mat, sprite_t **tex0, sprite_t **tex1)
{
    mat_get_tex(mat, tex0, tex1);
}

///@cond
bool __rdpq_mat_ext_get_bool(rdpq_mat_t *mat, uint32_t ext_key, bool *value)
{ return mat_ext_get(mat, ext_key, value, EXT_TYPE_BOOL); }
bool __rdpq_mat_ext_get_int(rdpq_mat_t *mat, uint32_t ext_key, uint32_t *value)
{ return mat_ext_get(mat, ext_key, value, EXT_TYPE_INT); }
bool __rdpq_mat_ext_get_string(rdpq_mat_t *mat, uint32_t ext_key, char **value)
{ return mat_ext_get(mat, ext_key, value, EXT_TYPE_STRING); }
bool __rdpq_mat_ext_get_float(rdpq_mat_t *mat, uint32_t ext_key, float *value)
{ return mat_ext_get(mat, ext_key, value, EXT_TYPE_FLOAT); }
///@endcond

/** Constructor to initialize the texture cache */
__attribute__((constructor))
void __rdpq_mat_init(void)
{
    // Initialize the texture cache
    hashtable_init(&tex_cache, 32, tex_cache_loader);
}
