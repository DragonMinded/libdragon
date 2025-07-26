/**
 * @file rdpq_mat_internal.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief RDP Command queue: material system
 * @ingroup rdpq
 */
#ifndef LIBDRAGON_RDPQ_MAT_INTERNAL_H
#define LIBDRAGON_RDPQ_MAT_INTERNAL_H

#include <stdint.h>

/// @cond
typedef struct sprite_s sprite_t;
/// @endcond

#define MATDBFLAG_AUTOLOAD              (1 << 0)    ///< Textures were loaded into memory automatically

#define MATFLAG_RMO_AA                  (1 << 0)    ///< Render mode override: antialias
#define MATFLAG_RMO_FOG                 (1 << 1)    ///< Render mode override: fog
#define MATFLAG_RMO_DITHERING           (1 << 2)    ///< Render mode override: dithering
#define MATFLAG_RMO_FILTERING           (1 << 3)    ///< Render mode override: filtering
#define MATFLAG_RMO_ZMODE               (1 << 4)    ///< Render mode override: Z mode
#define MATFLAG_RMO_ZPRIM               (1 << 5)    ///< Render mode override: Z primitive
#define MATFLAG_RMO_PERSP               (1 << 6)    ///< Render mode override: perspective correction
#define MATFLAG_RMO_ACMP                (1 << 7)    ///< Render mode override: alpha compare
#define MATFLAG_RMO_MASK                (0xFF)      ///< Mask for all render mode overrides

#define MATFLAG_TEXTURE                 (1 << 8)    ///< Texture is present
#define MATFLAG_BLENDER                 (1 << 9)    ///< Blender formula is present
#define MATFLAG_COMBINER                (1 << 10)   ///< Combiner formula is present
#define MATFLAG_UNIFORM_K4K5            (1 << 11)   ///< Combiner uniforms: K4 and K5
#define MATFLAG_UNIFORM_CHROMAKEY       (1 << 12)   ///< Combiner uniforms: key center and key scale
#define MATFLAG_UNIFORM_PRIMLODFRAC     (1 << 13)   ///< Combiner uniforms: primitive LOD fraction
#define MATFLAG_UNIFORM_ENV             (1 << 14)   ///< Combiner uniforms: env color
#define MATFLAG_UNIFORM_PRIM            (1 << 15)   ///< Combiner uniforms: prim color

/** Material database structure */
typedef struct rdpq_matdb_s {
    uint8_t id[3];                ///< ID: "MDB"
    uint8_t version;              ///< Version of the database
    uint16_t num_materials;       ///< Number of materials
    uint16_t flags;               ///< Flags
    uint8_t index[];              ///< Material index (name length + size)
} rdpq_matdb_t;

#endif
