#ifndef LIBDRAGON_RDPQ_MAT_INTERNAL_H
#define LIBDRAGON_RDPQ_MAT_INTERNAL_H

#include <stdint.h>

typedef struct sprite_s sprite_t;

#define MATDBFLAG_AUTOLOAD              (1 << 0)    // Textures were loaded into memory automatically

#define MATFLAG_RMO_AA                  (1 << 0)    // Render mode override: antialias
#define MATFLAG_RMO_FOG                 (1 << 1)    // Render mode override: fog
#define MATFLAG_RMO_DITHERING           (1 << 2)    // Render mode override: dithering
#define MATFLAG_RMO_FILTERING           (1 << 3)    // Render mode override: filtering
#define MATFLAG_RMO_ZMODE               (1 << 4)    // Render mode override: Z mode
#define MATFLAG_RMO_ZPRIM               (1 << 5)    // Render mode override: Z primitive
#define MATFLAG_RMO_PERSP               (1 << 6)    // Render mode override: perspective correction
#define MATFLAG_RMO_ACMP                (1 << 7)    // Render mode override: alpha compare
#define MATFLAG_RMO_MASK                (0xFF)
#define MATFLAG_TEXTURE                 (1 << 8)    // Texture is present
#define MATFLAG_BLENDER                 (1 << 9)    // Blender formula is present
#define MATFLAG_COMBINER                (1 << 10)   // Combiner formula is present
#define MATFLAG_UNIFORM_K4K5            (1 << 11)   // Combiner uniforms: K4 and K5
#define MATFLAG_UNIFORM_CHROMAKEY       (1 << 12)   // Combiner uniforms: key center and key scale
#define MATFLAG_UNIFORM_PRIMLODFRAC     (1 << 13)   // Combiner uniforms: primitive LOD fraction
#define MATFLAG_UNIFORM_ENV             (1 << 14)   // Combiner uniforms: env color
#define MATFLAG_UNIFORM_PRIM            (1 << 15)   // Combiner uniforms: prim color


/** Hashtable indexing materials */
typedef struct {
    uint32_t hash;                  ///< Hash value of the name of the material
    void *data;                     ///< Pointer to material data
} rdpq_matdb_hashtable_t;

/** Texture table */
typedef struct {
    uint32_t offset;                ///< Offset in the file to the texture data
    int size;                       ///< Size of the texture data
} rdpq_matdb_textable_t;

/** Entry of the cache of loaded textures */
typedef struct {
    uint32_t sprite_ptr  : 24;      ///< Pointer to the sprite data in RDRAM
    int8_t refcount      : 8;       ///< Reference count
} rdpq_matdb_texcache_t;

/** Material database */
typedef struct rdpq_matdb_header_s {
    char id[3];                         ///< ID: "MDB"
    uint8_t version;                    ///< Current version
    uint32_t meta_size;                 ///< Size of the header and all metadata
    int16_t num_textures;               ///< Number of textures
    int16_t num_materials;              ///< Number of materials
    uint16_t hash_prime;                ///< Prime number for hash
    uint16_t flags;                     ///< Flags
    rdpq_matdb_textable_t *textures;    ///< Table of textures
    rdpq_matdb_hashtable_t ht[];        ///< Hash table to lookup a material
} rdpq_matdb_header_t;

/** Material Database */
typedef struct rdpq_matdb_s {
    rdpq_matdb_header_t *head;          ///< Pointer to the header data (including all materials)
    int fd;                             ///< Open file descriptor
    rdpq_matdb_texcache_t texcache[];   ///< Texture cache
} rdpq_matdb_t;

#endif
