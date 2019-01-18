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
 * @see #get_accessories_present    
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
 * @see #identify_accessory
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

/** @brief SI Controller Data, GC */
typedef struct SI_condat_gc
{
    union
    {
        struct
        {
            /** @brief 64-bit data sent to or returned from SI */
            uint64_t data;
        };
        struct
        {
            unsigned err : 2;
            unsigned origin_unchecked : 1;
            unsigned start : 1;
            unsigned y : 1;
            unsigned x : 1;
            unsigned b : 1;
            unsigned a : 1;
            unsigned unused2 : 1;
            unsigned l : 1;
            unsigned r : 1;
            unsigned z : 1;
            unsigned up : 1;
            unsigned down : 1;
            unsigned right : 1;
            unsigned left : 1;
            unsigned stick_x : 8;
            unsigned stick_y : 8;

            unsigned cstick_x : 8;
            unsigned cstick_y : 8;
            unsigned analog_l : 8;
            unsigned analog_r : 8;
        };
    };
} _SI_condat_gc;

struct SI_origdat_gc {
    struct SI_condat_gc data;
    uint8_t deadzone0;
    uint8_t deadzone1;
};

/**
 * @brief Structure for interpreting SI responses
 */
typedef struct controller_data
{
    /** @brief Controller Data */
    struct SI_condat c[4];
    /** @brief Padding or GC data to allow mapping directly to a PIF block */
    struct SI_condat_gc gc[4];
} _controller_data;

struct controller_origin_data
{
    struct SI_origdat_gc gc[4];
};

#ifdef __cplusplus
extern "C" {
#endif

void controller_init();
void controller_read( struct controller_data * data);
void controller_read_gc( struct controller_data * data, const uint8_t rumble[4]);
void controller_read_gc_origin( struct controller_origin_data * data);
int get_controllers_present();
int get_accessories_present(struct controller_data * data);
void controller_scan();
struct controller_data get_keys_down();
struct controller_data get_keys_up();
struct controller_data get_keys_held();
struct controller_data get_keys_pressed();
int get_dpad_direction( int controller );
int read_mempak_address( int controller, uint16_t address, uint8_t *data );
int write_mempak_address( int controller, uint16_t address, uint8_t *data );
int identify_accessory( int controller );
void rumble_start( int controller );
void rumble_stop( int controller );
void execute_raw_command( int controller, int command, int bytesout, int bytesin, unsigned char *out, unsigned char *in );
int eeprom_present();
void eeprom_read(int block, uint8_t * const buf);
void eeprom_write(int block, const uint8_t * const data);

#ifdef __cplusplus
}
#endif

/** @} */ /* controller */

#endif
