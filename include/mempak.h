/**
 * @file mempak.h
 * @brief Controller Pak Filesystem Routines
 * @ingroup controllerpak
 */
#ifndef __LIBDRAGON_MEMPAK_H
#define __LIBDRAGON_MEMPAK_H

/**
 * @addtogroup controllerpak
 * @{
 */

/** @brief Size in bytes of a Controller Pak block */
#define MEMPAK_BLOCK_SIZE   256

/**
 * @brief Structure representing a save entry on a Controller Pak
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
     * @brief Name of this entry (UTF-8)
     *
     * The name is limited to 16 characters for name, and 4 characters for
     * extension (though most games only show the first character of the
     * extension). The extension is separated from the name using a dot.
     * The valid characters set is very limited, and contains
     * only a subset of ASCII or Katakana. The complete character map is:
     * 
     * <pre>
     * 	0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!"#'*+,-./:=?@
     *  。゛゜ァィゥェォッャュョヲンアイウエオカキクケコサシスセソタ
     *  チツテトナニヌネノハヒフヘホマミムメモヤユヨラリルレロワガギグゲ
     *  ゴザジズゼゾダヂヅデドバビブベボパピプペポ
     * </pre>
     *
     * The space character is also allowed.  Any other character will be
     * converted to a space before writing to the Controller Pak.
     * 
     * The buffer here is longer because it allows for UTF-8 encoding of the
     * Japanese characters.
     */
    char name[16*3 + 1 + 4*3 + 1];
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

/** @} */ /* controllerpak */

#endif
