/**
 * @file rdpq_mat.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief RDP Command queue: material system
 * @ingroup rdpq
 */
#ifndef LIBDRAGON_RDPQ_MAT_H
#define LIBDRAGON_RDPQ_MAT_H

#ifdef __cplusplus
extern "C" {
#endif

///@cond
typedef struct rdpq_matdb_s rdpq_matdb_t;
typedef void* rdpq_mat_t;
///@endcond

/**
 * @brief Open a material DB file and load it into memory
 * 
 * This function loads a Material DB file. By default, textures are not
 * loaded into RDRAM until requested by the user. This is done to save
 * memory when the material database is large. 
 * 
 * @param filename              Path to the material database file
 * @return rdpq_matdb_t*        Pointer to the material database
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
 * @return rdpq_mat_t*      Pointer to the material, or NULL if not found
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
