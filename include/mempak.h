/**
 * @file mempak.h
 * @brief Controller Pak Filesystem Routines
 * @ingroup controllerpak
 */
#ifndef __LIBDRAGON_MEMPAK_H
#define __LIBDRAGON_MEMPAK_H

/**
 * @defgroup controllerpak Controller Pak Filesystem Routines
 * @ingroup controller
 * @brief Managed Controller Pak interface.
 *
 * The Controller Pak system is a subsystem of the @ref controller. Before attempting to
 * read from or write to a Controller Pak, be sure you have initialized the Joypad subsystem
 * with #joypad_init and verified that you have a Controller Pak in the correct controller
 * using #joypad_get_accessory_type.
 *
 * To read and write to the Controller Pak in an organized way compatible with official software,
 * first check that the Controller Pak is valid using #validate_mempak.  If the Controller Pak is
 * invalid, it will need to be formatted using #format_mempak.  Once the Controller Pak is
 * considered valid, existing notes can be enumerated using #get_mempak_entry.  To
 * read the data associated with a note, use #read_mempak_entry_data.  To write a
 * new note to the Controller Pak, use #write_mempak_entry_data.  Note that there is no append
 * functionality so if a note is being updated, ensure you have deleted the old note
 * first using #delete_mempak_entry.  Code should be careful to check how many blocks
 * are free before writing using #get_mempak_free_space.
 *
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

/**
 * @brief Read a sector from a Controller Pak
 *
 * This will read a sector from a Controller Pak. Sectors on Controller Paks are always 256 bytes
 * in size.
 *
 * @param[in]  controller
 *             The controller (0-3) to read a sector from
 * @param[in]  sector
 *             The sector (0-127) to read from
 * @param[out] sector_data
 *             Buffer to place 256 read bytes of data
 *
 * @retval 0 if reading was successful
 * @retval -1 if the sector was out of bounds or sector_data was null
 * @retval -2 if there was an error reading part of a sector
 */
int read_mempak_sector( int controller, int sector, uint8_t *sector_data );

/**
 * @brief Write a sector to a Controller Pak
 *
 * This will write a sector to a Controller Pak.  Sectors on Controller Paks are always 256 bytes
 * in size.
 *
 * @param[in]  controller
 *             The controller (0-3) to write a sector to
 * @param[in]  sector
 *             The sector (0-127) to write to
 * @param[out] sector_data
 *             Buffer containing 256 bytes of data to write to sector
 *
 * @retval 0 if writing was successful
 * @retval -1 if the sector was out of bounds or sector_data was null
 * @retval -2 if there was an error writing part of a sector
 */
int write_mempak_sector( int controller, int sector, uint8_t *sector_data );

/**
 * @brief Return whether a Controller Pak is valid
 *
 * This function will return whether the Controller Pak in a particular controller
 * is formatted and valid.
 *
 * @param[in] controller
 *            The controller (0-3) to validate
 *
 * @retval 0 if the Controller Pak is valid and ready to be used
 * @retval -2 if the Controller Pak is not present or couldn't be read
 * @retval -3 if the Controller Pak is bad or unformatted
 */
int validate_mempak( int controller );

/**
 * @brief Return the number of free blocks on a Controller Pak
 *
 * Note that a block is identical in size to a sector.  To calculate the number of
 * bytes free, multiply the return of this function by #MEMPAK_BLOCK_SIZE.
 *
 * @param[in] controller
 *            The controller (0-3) to read the free space from
 *
 * @return The number of blocks free on the memory card or a negative number on failure
 */
int get_mempak_free_space( int controller );

/**
 * @brief Read an entry on a Controller Pak
 *
 * Given an entry index (0-15), return the entry as found on the Controller Pak.  If
 * the entry is blank or invalid, the valid flag is cleared.
 *
 * @param[in]  controller
 *             The controller (0-3) from which the entry should be read
 * @param[in]  entry
 *             The entry index (0-15) to read
 * @param[out] entry_data
 *             Structure containing information on the entry
 *
 * @retval 0 if the entry was read successfully
 * @retval -1 if the entry is out of bounds or entry_data is null
 * @retval -2 if the Controller Pak is bad or not present
 */
int get_mempak_entry( int controller, int entry, entry_structure_t *entry_data );

/**
 * @brief Format a Controller Pak
 *
 * Formats a Controller Pak. This should only be done to totally wipe and re-initialize
 * the filesystem in case of a blank or corrupt Controller Pak after a repair has failed.
 *
 * @param[in] controller
 *            The Controller (0-3) that the Controller Pak is inserted.
 *
 * @retval 0 if the Controller Pak was formatted successfully.
 * @retval -2 if the Controller Pak was not present or couldn't be formatted.
 */
int format_mempak( int controller );

/**
 * @brief Read the data associated with an entry on a Controller Pak
 *
 * Given a valid Controller Pak entry fetched by get_mempak_entry, retrieves the contents
 * of the entry.  The calling function must ensure that enough room is available in
 * the passed in buffer for the entire entry.  The entry structure itself contains
 * the number of blocks used to store the data which can be multiplied by
 * #MEMPAK_BLOCK_SIZE to calculate the size of the buffer needed.
 *
 * @param[in]  controller
 *             The controller (0-3) to read the entry data from
 * @param[in]  entry
 *             The entry structure associated with the data to be read.  An entry
 *             structure can be fetched based on index using #get_mempak_entry
 * @param[out] data
 *             The data associated with an entry
 *
 * @retval 0 if the entry was successfully read
 * @retval -1 if input parameters were out of bounds or the entry was corrupted somehow
 * @retval -2 if the Controller Pak was not present or bad
 * @retval -3 if the data couldn't be read
 */
int read_mempak_entry_data( int controller, entry_structure_t *entry, uint8_t *data );

/**
 * @brief Write associated data to a Controller Pak entry
 *
 * Given a Controller Pak entry structure with a valid region, name and block count, writes the
 * entry and associated data to the Controller Pak.  This function will not overwrite any existing
 * user data.  To update an existing entry, use #delete_mempak_entry followed by
 * #write_mempak_entry_data with the same entry structure.
 *
 * @param[in] controller
 *            The controller (0-3) to write the entry and data to
 * @param[in] entry
 *            The entry structure containing a region, name and block count
 * @param[in] data
 *            The associated data to write to to the created entry
 *
 * @retval 0 if the entry was created and written successfully
 * @retval -1 if the parameters were invalid or the note has no length
 * @retval -2 if the Controller Pak wasn't present or was bad
 * @retval -3 if there was an error writing to the Controller Pak
 * @retval -4 if there wasn't enough space to store the note
 * @retval -5 if there is no room in the TOC to add a new entry
 */
int write_mempak_entry_data( int controller, entry_structure_t *entry, uint8_t *data );

/**
 * @brief Delete a Controller Pak entry and associated data
 *
 * Given a valid Controller Pak entry fetched by #get_mempak_entry, removes the entry and frees
 * all associated blocks.
 *
 * @param[in] controller
 *            The controller (0-3) to delete the note from
 * @param[in] entry
 *            The entry structure that is to be deleted from the Controller Pak
 *
 * @retval 0 if the entry was deleted successfully
 * @retval -1 if the entry was invalid
 * @retval -2 if the Controller Pak was bad or not present
 */
int delete_mempak_entry( int controller, entry_structure_t *entry );

#ifdef __cplusplus
}
#endif

/** @} */ /* controllerpak */

#endif
