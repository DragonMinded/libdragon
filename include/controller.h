#ifndef __LIBDRAGON_CONTROLLER_H
#define __LIBDRAGON_CONTROLLER_H

#define CONTROLLER_1_INSERTED   0xF000
#define CONTROLLER_2_INSERTED   0x0F00
#define CONTROLLER_3_INSERTED   0x00F0
#define CONTROLLER_4_INSERTED   0x000F

#define ACCESSORY_NONE          0
#define ACCESSORY_MEMPAK        1
#define ACCESSORY_RUMBLEPAK     2

#define ERROR_NONE          0x0
#define ERROR_BAD_COMMAND   0x1
#define ERROR_NOT_PRESENT   0x2

#define MEMPAK_BLOCK_SIZE   256

typedef struct SI_condat 
{
    unsigned : 16;
    unsigned err : 2; // error status
    unsigned : 14;

    union 
    {
        struct 
        {
            unsigned int data : 32;
        };
        struct 
        {
            unsigned A : 1;
            unsigned B : 1;
            unsigned Z : 1;
            unsigned start : 1;
            unsigned up : 1;
            unsigned down : 1;
            unsigned left : 1;
            unsigned right : 1;
            unsigned : 2;
            unsigned L : 1;
            unsigned R : 1;
            unsigned C_up : 1;
            unsigned C_down : 1;
            unsigned C_left : 1;
            unsigned C_right : 1;
            signed x : 8;
            signed y : 8;
        };
    };
} _SI_condat;

typedef struct controller_data 
{
    struct 
    {
        struct SI_condat c[4];
        unsigned long unused[4*8]; // to map directly to PIF block
    };
} _controller_data;

typedef struct entry_structure
{
    uint32_t vendor;
    uint16_t game_id;
    uint16_t inode;
    uint8_t region;
    uint8_t blocks;
    uint8_t valid;
    uint8_t entry_id;
    char name[19];
} entry_structure_t;

/* Initialize the controller system. */
void controller_init();

/* Read the controller status immediately and return results to data.  If
   calling this function, one should not also call controller_scan as this
   does not update the internal state of controllers. */
void controller_read( struct controller_data * data);

/* Read the controller status immediately and return a bitmask representing
   which controllers are present in the system.  Note that this does not update
   the current internal state of the controllers. */
int get_controllers_present();

/* Read the controller status immediately and return a bitmask representing
   which controllers have inserted accessories */
int get_accessories_present();

/* Scan the four controller ports and calculate the buttons state.  This
   must be called before calling any of the below functions. */
void controller_scan();

/* Return keys pressed since last detection.  This returns a standard
   controller_data struct identical to controller_read.  However, buttons
   are only set if they were pressed down since the last controller_scan() */
struct controller_data get_keys_down();

/* Similar to get_keys_down, except reports keys that were let go since the
   last controller_scan(). */
struct controller_data get_keys_up();

/* Similar to get_keys_down, except reports keys that have been held down for
   at least one controller_scan() query. */
struct controller_data get_keys_held();

/* Returns any key that is currently held down, regardless of the previous
   state. */
struct controller_data get_keys_pressed();

/* Return the direction of the DPAD specified in controller.  Follows standard
   polar coordinates, where 0 = 0, pi/4 = 1, pi/2 = 2, etc...  Returns -1 when
   not pressed.  Must be used in conjunction with controller_scan() */
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

#endif
