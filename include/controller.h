/**
 * @file controller.h
 * @brief Controller Subsystem
 * @ingroup controller
 */

#ifndef __LIBDRAGON_CONTROLLER_H
#define __LIBDRAGON_CONTROLLER_H

/**
 * @addtogroup controller
 * @{
 */

/**
 * @name Bitmasks for controller status
 * @see #get_controllers_present
 * @{
 */

/** @brief Controller 1 Inserted */
#define CONTROLLER_1_INSERTED   0xF000
/** @brief Controller 2 Inserted */
#define CONTROLLER_2_INSERTED   0x0F00
/** @brief Controller 3 Inserted */
#define CONTROLLER_3_INSERTED   0x00F0
/** @brief Controller 4 Inserted */
#define CONTROLLER_4_INSERTED   0x000F
/** @} */

/**
 * @name Accessory ID Values
 * @{
 */

/** @brief No accessory present */
#define ACCESSORY_NONE          0
/** @brief Mempak present */
#define ACCESSORY_MEMPAK        1
/** @brief Rumblepak present */
#define ACCESSORY_RUMBLEPAK     2
/** @brief VRU present */
#define ACCESSORY_VRU           3
/** @} */

/**
 * @name SI Error Values
 * @{
 */

/** @brief No error occured */
#define ERROR_NONE          0x0
/** @brief Command not recognized or malformed */
#define ERROR_BAD_COMMAND   0x1
/** @brief Controller not present */
#define ERROR_NOT_PRESENT   0x2
/** @} */

/** @brief Size in bytes of a Mempak block */
#define MEMPAK_BLOCK_SIZE   256

/** @brief SI Controller Data */
typedef struct SI_condat
{
    /** @brief Unused padding bits */
    unsigned : 16;
    /** @brief Status of the last command */
    unsigned err : 2;
    /** @brief Unused padding bits */
    unsigned : 14;

    union
    {
        struct
        {
            /** @brief 32-bit data sent to or returned from SI */
            unsigned int data : 32;
        };
        struct
        {
            /** @brief State of the A button */
            unsigned A : 1;
            /** @brief State of the B button */
            unsigned B : 1;
            /** @brief State of the Z button */
            unsigned Z : 1;
            /** @brief State of the start button */
            unsigned start : 1;
            /** @brief State of the up button */
            unsigned up : 1;
            /** @brief State of the down button */
            unsigned down : 1;
            /** @brief State of the left button */
            unsigned left : 1;
            /** @brief State of the right button */
            unsigned right : 1;
            /** @brief Unused padding bits */
            unsigned : 2;
            /** @brief State of the L button */
            unsigned L : 1;
            /** @brief State of the R button */
            unsigned R : 1;
            /** @brief State of the C up button */
            unsigned C_up : 1;
            /** @brief State of the C down button */
            unsigned C_down : 1;
            /** @brief State of the C left button */
            unsigned C_left : 1;
            /** @brief State of the C right button */
            unsigned C_right : 1;
            /** @brief State of the X button */
            signed x : 8;
            /** @brief State of the Y button */
            signed y : 8;
        };
    };
} _SI_condat;

/**
 * @brief Structure for interpreting SI responses
 */
typedef struct controller_data
{
    /** @brief Controller Data */
    struct SI_condat c[4];
    /** @brief Padding to allow mapping directly to a PIF block */
    unsigned long unused[4*8];
} _controller_data;

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
    /** @brief Number of blocks used by this entry */
    uint8_t blocks;
    /** @brief Validity of this entry. */
    uint8_t valid;
    /** @brief ID of this entry */
    uint8_t entry_id;
    /**
     * @brief Name of this entry
     * @see #__n64_to_ascii and #__ascii_to_n64
     */
    char name[19];
} entry_structure_t;

#ifdef __cplusplus
extern "C" {
#endif

void controller_init();
void controller_read( struct controller_data * data);
int get_controllers_present();
int get_accessories_present();
void controller_scan();
struct controller_data get_keys_down();
struct controller_data get_keys_up();
struct controller_data get_keys_held();
struct controller_data get_keys_pressed();
int get_dpad_direction( int controller );

/* Reads a 32 byte aligned address from a controller and places 32 read bytes
   into data */
int read_mempak_address( int controller, uint16_t address, uint8_t *data );

/* Writes 32 bytes to a 32 byte aligned address */
int write_mempak_address( int controller, uint16_t address, uint8_t *data );

/* Given a controller, identify the particular accessory type inserted.  See
   accessory defines above */
int identify_accessory( int controller );

/* Start rumble on a particular controller */
void rumble_start( int controller );

/* Stop rumble on a particular controller */
void rumble_stop( int controller );
void execute_raw_command( int controller, int command, int bytesout, int bytesin, unsigned char *out, unsigned char *in );

/* Read a sector off of a memory card.  Valid sector numbers are 0-127 */
int read_mempak_sector( int controller, int sector, uint8_t *sector_data );

/* Write a sector to a memory card.  Same restriction as above */
int write_mempak_sector( int controller, int sector, uint8_t *sector_data );

/* Return whether a mempak is formatted and valid */
int validate_mempak( int controller );

/* Return the number of blocks free on the mempak */
int get_mempak_free_space( int controller );

/* Given an entry index (0-15), return the entry as found on the mempak.  If
   the entry is blank or invalid, the valid flag is cleared. */
int get_mempak_entry( int controller, int entry, entry_structure_t *entry_data );

/* Formats a mempak.  Should only be done to wipe a mempak or to initialize
   the filesystem in case of a blank or corrupt mempak */
int format_mempak( int controller );

/* Given a valid mempak entry fetched by get_mempak_entry, retrieves the contents
   of the entry.  The calling function must ensure that enough room is available in
   the passed in buffer for the entire entry.  The entry structure itself contains
   the number of blocks used to store the data which can be multiplied by
   MEMPAK_BLOCK_SIZE to calculate the size of the buffer needed */
int read_mempak_entry_data( int controller, entry_structure_t *entry, uint8_t *data );

/* Given a mempak entry structure with a valid region, name and block count, writes the
   data to a free entry on the mempak.  All other values on the entry structure are
   assumed to be uninitialized.  This function does no checking for existing entries
   of the same name.  To remove an old record, use the delete_mempak_entry function. */
int write_mempak_entry_data( int controller, entry_structure_t *entry, uint8_t *data );

/* Given a valid mempak entry fetched by get_mempak_entry, removes the entry and frees
   all associated blocks. */
int delete_mempak_entry( int controller, entry_structure_t *entry );
unsigned int eeprom_status();
unsigned long long eeprom_read(int block);
unsigned int eeprom_write(int block, unsigned long long data);

#ifdef __cplusplus
}
#endif

/** @} */ /* controller */

#endif
