/**
 * @file rdpq_mat.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef LIBDRAGON_RDPQ_MAT_H
#define LIBDRAGON_RDPQ_MAT_H

#ifdef __cplusplus
extern "C" {
#endif

///@cond
typedef struct rdpq_matdb_s rdpq_matdb_t;
///@endcond

/**
 * @brief Open a material DB file and load it into memory
 * 
 * This function opens a Material DB file. By default, textures are not
 * loaded into RDRAM until requested by the user. This is done to save
 * memory when the material database is large. 
 * 
 * By setting @p autoload to true, all textures will be loaded into memory
 * immediately. This avoids the need to call #rdpq_matdb_load for each material,
 * but it may consume a lot of memory.
 * 
 * @param filename              Path to the material database file
 * @param autoload              If true, all textures are loaded into memory immediately
 * @return rdpq_matdb_t*        Pointer to the material database
 */
rdpq_matdb_t* rdpq_matdb_open(const char *filename, bool autoload);

/** @brief Close a material database */
void rdpq_matdb_close(rdpq_matdb_t* mdb);

/**
 * @brief Load textures associated with a material into RDRAM
 * 
 * This function loads the textures associated with a material into RDRAM.
 * It must be called before #rdpq_matdb_begin if the material contains
 * textures (it is a no-op if it doesn't, so it is safe to always call it
 * just in case).
 * 
 * Notice that each call to #rdpq_matdb_load must be paired by a call to
 * #rdpq_matdb_unload, to unload the textures, before the material DB is closed.
 * This is done to enforce proper resource management, and avoid silent memory
 * leaks.
 * 
 * You can call #rdpq_matdb_load multiple times for the same material, but
 * you must call #rdpq_matdb_unload the same number of times.
 * 
 * @param mdb               Material database
 * @param mat_name          Name of the material whose textures must be loaded
 */
void rdpq_matdb_load(rdpq_matdb_t* mdb, const char *mat_name);

/** @brief Unload the material */
void rdpq_matdb_unload(rdpq_matdb_t* mdb, const char *mat_name);

/**
 * @brief Activate a material by configuring the RDP state and uploading textures to TMEM
 * 
 * This function configures the RDP to prepare drawing primitives (triangles or
 * rectangles) with the material specified by @p mat_name. It sets the RDP state
 * to the one specified in the material database, and uploads textures to TMEM.
 * 
 * After drawing the primitives with this material, you must call #rdpq_matdb_end
 * to restore the previous RDP state. In fact, the rdpq material system allows
 * for render mode overrides; for instance, a single material might need Z writes
 * to be disabled, but then any further material would need to simply revert
 * back to the previous state configured for the scene.
 * 
 * Before calling #rdpq_matdb_begin, you must call #rdpq_matdb_load to load the
 * textures associated with the material (unless you loaded them all
 * when calling #rdpq_matdb_open, by setting @p autoload to true).
 * 
 * @note If you want, you can register a block for this call, either by itself
 *       or as part of a larger block. Notice that #rdpq_matdb_end instead
 *         
 * 
 * @param mdb               Material database
 * @param mat_name          Name of the material to activate
 */
void rdpq_matdb_begin(rdpq_matdb_t* mdb, const char *mat_name);

/** @brief Finish rendering with the current material */
void rdpq_matdb_end(rdpq_matdb_t* mdb, const char *mat_name);

/** @brief Dump debug information about a material */
void rdpq_matdb_debug_dump(rdpq_matdb_t* mdb, const char *mat_name);

#ifdef __cplusplus
}
#endif

#endif
