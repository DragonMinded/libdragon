/**
 * @file mempak.h
 * @brief Mempak Filesystem Routines
 * @ingroup mempak
 */
#ifndef __LIBDRAGON_MEMPAK_H
#define __LIBDRAGON_MEMPAK_H

/**
 * @addtogroup mempak
 * @{
 */

/** @brief Size in bytes of a Mempak block */
#define MEMPAK_BLOCK_SIZE   256

/**
 * @brief Structure representing a save entry in a mempak
 */
typedef struct entry_structure
{
    /** @brief Vendor ID */
    uint32_t vendor;
    /** @brief Game ID */
    uint16_t game_id;
    /** @brief Inode pointer */
    uint16_t inode;
    /** @brief Intended region */
    uint8_t region;
    /** @brief Number of blocks used by this entry.
     * @see MEMPAK_BLOCK_SIZE */
    uint8_t blocks;
    /** @brief Validity of this entry. */
    uint8_t valid;
    /** @brief ID of this entry */
    uint8_t entry_id;
    /**
     * @brief Name of this entry
     *
     * The complete list of valid ASCII characters in a note name is:
     *
     * <pre>
     * ABCDEFGHIJKLMNOPQRSTUVWXYZ!"#`*+,-./:=?\@
     * </pre>
     *
     * The space character is also allowed.  Any other character will be
     * converted to a space before writing to the mempak.
     *
     * @see #__n64_to_ascii and #__ascii_to_n64
     */
    char name[19];
} entry_structure_t;

#ifdef __cplusplus
extern "C" {
#endif

int read_mempak_sector( int controller, int sector, uint8_t *sector_data );
int write_mempak_sector( int controller, int sector, uint8_t *sector_data );
int validate_mempak( int controller );
int get_mempak_free_space( int controller );
int get_mempak_entry( int controller, int entry, entry_structure_t *entry_data );
int format_mempak( int controller );
int read_mempak_entry_data( int controller, entry_structure_t *entry, uint8_t *data );
int write_mempak_entry_data( int controller, entry_structure_t *entry, uint8_t *data );
int delete_mempak_entry( int controller, entry_structure_t *entry );

#ifdef __cplusplus
}
#endif

/** @} */ /* mempak */

#endif
