/**
 * @file rdpq_mat.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief RDP Command queue: material system
 * @ingroup rdpq
 * 
 * The RDP material system provides a high-level API for managing complex rendering
 * state by organizing related RDP settings into reusable "materials". A material
 * encapsulates textures, combiners, blending modes, and other RDP state that
 * define how primitives should be rendered, allowing for efficient switching
 * between different visual appearances.
 * 
 * Materials can contain textures, color combiner formulas, blending modes,
 * antialiasing settings, fog parameters, Z-buffer configuration, and various
 * render mode overrides. They also support an extensible system for storing
 * custom key-value pairs, making them suitable for advanced rendering pipelines
 * and integration with external tools.
 * 
 * The material system is designed for performance, with efficient binary formats
 * for storage and fast lookup mechanisms. Materials are typically created using
 * the mkmaterial tool and stored in material database (.mdb) files, which can contain
 * multiple materials with fast name-based lookup.
 * 
 * ## Basic Usage
 * 
 * The typical workflow involves:
 * 1. Configure the texture database path with #rdpq_mat_set_texture_path. This
 *    was created via mkmaterial and should normally be stored in the ROM (though
 *    any valid filesystem can be used, including SD, etc.).
 * 2. Open a material database using #rdpq_matdb_open
 * 3. Load specific materials with #rdpq_matdb_load, which will also load its
 *    textures into RDRAM. This should be done when eg a mesh is loaded and
 *    the material is needed for rendering.
 * 4. Apply materials before rendering with #rdpq_mat_draw_begin
 * 5. Render your primitives (triangles, sprites, etc.)
 * 6. Clean up the material state with #rdpq_mat_draw_end
 * 7. Free materials when done with #rdpq_mat_free
 * 8. Close the material database with #rdpq_matdb_close when not needed anymore.
 * 
 * ## Source material files
 * 
 * Materials (.mat files) are INI files that describe all the properties of one
 * or multiple materials. They can be processed by the mkmaterial tool to
 * generate the binary material database (.mdb) files used at runtime.
 * An alternative representation in json format (.jmat files) is also supported,
 * which is useful when generating materials via a tool (eg: exporting from a 3D editor).
 * 
 * ## Material Database Files
 * 
 * Material databases (.mdb files) are binary files that efficiently store
 * multiple materials with metadata for fast loading and lookup. You can use
 * a single material database for a whole scene, or just one per model,
 * depending on how your asset pipeline is structured.
 * 
 * ## Texture Management
 * 
 * Materials reference textures by hash, allowing the same texture to be shared
 * across multiple materials efficiently. Textures are loaded on-demand and
 * cached automatically. Configure the texture search path using
 * #rdpq_mat_set_texture_path.
 * 
 * ## Extension System
 * 
 * Materials support custom extensions through a key-value system that can
 * store integers, floats, strings, and booleans. This allows materials to
 * carry application-specific data or parameters for custom shaders and
 * rendering techniques.
 */
#ifndef LIBDRAGON_RDPQ_MAT_H
#define LIBDRAGON_RDPQ_MAT_H

#include <stdint.h>
#include <stdbool.h>
#include "string_hash.h"

#ifdef __cplusplus
extern "C" {
#endif

///@cond
typedef struct rdpq_matdb_s rdpq_matdb_t;
typedef void* rdpq_mat_t;

bool __rdpq_mat_ext_get_bool(rdpq_mat_t *mat, uint32_t ext_key, bool *value);
bool __rdpq_mat_ext_get_int(rdpq_mat_t *mat, uint32_t ext_key, uint32_t *value);
bool __rdpq_mat_ext_get_string(rdpq_mat_t *mat, uint32_t ext_key, char **value);
bool __rdpq_mat_ext_get_float(rdpq_mat_t *mat, uint32_t ext_key, float *value);
///@endcond

/**
 * @brief Open a material DB file and load it into memory
 * 
 * This function loads a Material DB file. By default, textures are not
 * loaded into RDRAM until requested by the user. This is done to save
 * memory when the material database is large. 
 * 
 * @param filename              Path to the material database file
 * @return                      Pointer to the material database
 */
rdpq_matdb_t* rdpq_matdb_open(const char *filename);

/** @brief Close a material database */
void rdpq_matdb_close(rdpq_matdb_t* mdb);

/**
 * @brief Load a material from the material database
 * 
 * This function retrieves a material from the material database by its name.
 * If the material does not exist, it returns NULL.
 * 
 * Textures associated with the material are also loaded into RDRAM.
 * 
 * Make sure to call #rdpq_mat_free when done with the material,
 * 
 * @param mdb               Material database
 * @param mat_name          Name of the material to retrieve. If NULL,
 *                          the first material in the database is returned.
 * @return                  Pointer to the material, or NULL if not found
 */
rdpq_mat_t* rdpq_matdb_load(rdpq_matdb_t* mdb, const char *mat_name);

/** 
 * @brief Load a single material from a raw buffer
 * 
 * This function is useful to load a material from its raw binary representation,
 * stored outside of a material database. The raw representation is created
 * by mkmaterial when using the --raw-material option, and can be used to
 * embed the material as part of a larger bundle (eg: a mesh file).
 * 
 * If you use a material database (.mdb) you don't need this function; use
 * #rdpq_matdb_load instead.
 * 
 * @param buf              Pointer to the raw material buffer
 * @param size             Size of the buffer
 * @return Loaded material
 */
rdpq_mat_t* rdpq_mat_load_buf(void *buf, int size);

/**
 * @brief Activate a material by configuring the RDP state and uploading textures to TMEM
 * 
 * This function configures the RDP to prepare drawing primitives (triangles or
 * rectangles) with the material specified by @p mat. It sets the RDP state
 * to the one specified in the material, and uploads textures to TMEM.
 * 
 * After drawing the primitives with this material, you must call #rdpq_mat_draw_end
 * to restore the previous RDP state. In fact, the rdpq material system allows
 * for render mode overrides; for instance, a single material might need Z writes
 * to be disabled, but then any further material would need to simply revert
 * back to the previous state configured for the scene.
 * 
 * @param mat               Material to activate
 * 
 * @see #rdpq_mat_draw_end
 */
void rdpq_mat_draw_begin(rdpq_mat_t *mat);

/**
 * @brief Deactivate a material after drawing with it
 * 
 * This function ends the use of the specified material and restores the previous
 * RDP state.
 * 
 * @param mat               Material to deactivate
 */
void rdpq_mat_draw_end(rdpq_mat_t *mat);

/**
 * @brief Get a boolean extension value from the material
 * 
 * This function retrieves a boolean extension value from the
 * material, specified by its name. If the name does not exist, it returns false.
 * 
 * Note that the name is case-sensitive, and must be specified without the "ext."
 * prefix. So for instance, if the material file contains an extension key called
 * "ext.mygame.has_alpha", you should call this function with "mygame.has_alpha".
 * 
 * @param mat               Material to query
 * @param name              Name of the boolean value to retrieve
 * @param value             Pointer to store the result
 * @return true if the name exists, false otherwise
 * 
 * @hideinitializer
 */
#define rdpq_mat_ext_get_bool(mat, name, value) \
    __rdpq_mat_ext_get_bool(mat, string_hash(name) & 0xFFFF, value)

/**
 * @brief Get an integer extension value from the material
 * 
 * This function retrieves an integer extension value from the
 * material, specified by its name. If the name does not exist, it returns false.
 * 
 * Note that the name is case-sensitive, and must be specified without the "ext."
 * prefix. So for instance, if the material file contains an extension key called
 * "ext.mygame.has_alpha", you should call this function with "mygame.has_alpha".
 * 
 * @param mat               Material to query
 * @param name              Name of the boolean value to retrieve
 * @param value             Pointer to store the result
 * @return true if the name exists, false otherwise
 * 
 * @hideinitializer
 */
#define rdpq_mat_ext_get_int(mat, name, value) \
    __rdpq_mat_ext_get_int(mat, string_hash(name) & 0xFFFF, value)

/**
 * @brief Get a string extension value from the material
 * 
 * This function retrieves a string extension value from the
 * material, specified by its name. If the name does not exist, it returns false.
 * The string is NULL terminated.
 * 
 * Note that the name is case-sensitive, and must be specified without the "ext."
 * prefix. So for instance, if the material file contains an extension key called
 * "ext.mygame.has_alpha", you should call this function with "mygame.has_alpha".
 * 
 * @param mat               Material to query
 * @param name              Name of the boolean value to retrieve
 * @param value             Pointer to store the result
 * @return true if the name exists, false otherwise
 * 
 * @hideinitializer
 */
#define rdpq_mat_ext_get_string(mat, name, value) \
    __rdpq_mat_ext_get_string(mat, string_hash(name) & 0xFFFF, value)

/**
 * @brief Get a float extension value from the material
 * 
 * This function retrieves a float extension value from the
 * material, specified by its name. If the name does not exist, it returns false.
 * 
 * Note that the name is case-sensitive, and must be specified without the "ext."
 * prefix. So for instance, if the material file contains an extension key called
 * "ext.mygame.has_alpha", you should call this function with "mygame.has_alpha".
 * 
 * @param mat               Material to query
 * @param name              Name of the float value to retrieve
 * @param value             Pointer to store the result
 * @return true if the name exists, false otherwise
 * 
 * @hideinitializer
 */
#define rdpq_mat_ext_get_float(mat, name, value) \
    __rdpq_mat_ext_get_float(mat, string_hash(name) & 0xFFFF, value)


/**
 * @brief Free a material loaded with #rdpq_matdb_load or #rdpq_mat_load_buf
 * 
 * This function releases the textures associated with the material. Notice
 * that the material itself is not freed: in fact, it is either part of the
 * material database (that will be freed via #rdpq_matdb_close), or
 * it is a standalone material loaded with #rdpq_mat_load_buf and the
 * caller will be responsible for freeing the memory.
 * 
 * @param mat        Material to free
 */
void rdpq_mat_free(rdpq_mat_t *mat);


/**
 * @brief Set the path in which the texture DB is stored
 * 
 * The texture database is a directory in an accessible filesystem
 * (normally, in ROM) where all the textures using the rdpq material
 * system are stored. The database is created by mkmaterial while
 * processing materials.
 * 
 * This function informs the rdpq_mat library where the database
 * can be accessed.
 * 
 * @param path          Path to the texture database directory,
 *                      including filesystem prefix (eg: "rom:/textures")
 */
void rdpq_mat_set_texture_path(const char *path);

#ifdef __cplusplus
}
#endif

#endif
