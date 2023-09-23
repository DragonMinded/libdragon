/**
 * @file controller.h
 * @brief Controller Subsystem
 * @ingroup controller
 */
#ifndef __LIBDRAGON_CONTROLLER_H
#define __LIBDRAGON_CONTROLLER_H

#include <stdint.h>

#include "joybus.h"
#include "joybus_accessory.h"
#include "joypad.h"

/**
 * @addtogroup controller
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 * Deprecated API
 * 
 * The following API is deprecated and will be removed in the future.
 * This API is being kept for compatibility with existing code. The
 * implementation is now based on the Joypad Subsystem, which fixes
 * several deficiencies and implements first-class support for GameCube
 * controllers. The Controller Subsystem will emit deprecation warnings
 * when used and suggest replacement functions and data structures.
 ****************************************************************************/

/**
 * @name Bitmasks for controller status
 * 
 * @deprecated Use #joypad_is_connected instead.
 * 
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
 * 
 * @deprecated Use #joypad_accessory_type_t instead.
 * 
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
/** @brief Transferpak present */
#define ACCESSORY_TRANSFERPAK    4
/** @} */

/**
 * @name SI Error Values
 * 
 * @deprecated These values are no longer used. They will be removed in the future.
 * 
 * @{
 */
/** @brief No error occured */
#define ERROR_NONE          0x0
/** @brief Command not recognized or malformed */
#define ERROR_BAD_COMMAND   0x1
/** @brief Controller not present */
#define ERROR_NOT_PRESENT   0x2
/** @} */

/**
 * @brief SI Nintendo 64 controller data
 * 
 * Data structure for Joybus response to `0x01` (Read N64 controller state) command.
 * 
 * @deprecated The @ref joypad "Joypad Subsystem" now automatically normalizes
 *             Nintendo 64 and GameCube controller inputs. This structure is no
 *             longer needed. Use #joypad_inputs_t instead.
 */
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
            /** @brief State of the analog stick (X axis) */
            signed x : 8;
            /** @brief State of the analog stick (Y axis) */
            signed y : 8;
        };
    };
} _SI_condat;

/**
 * @brief SI GameCube controller data.
 * 
 * Data structure for Joybus response to `0x40` (Read GC controller state) command.
 * 
 * @deprecated The @ref joypad "Joypad Subsystem" now automatically normalizes
 *             Nintendo 64 and GameCube controller inputs. This structure is no
 *             longer needed. Use #joypad_inputs_t instead.
 */
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

/**
 * @brief SI GameCube controller origin data.
 * 
 * Data structure for Joybus response to `0x41` (Read GC controller origin) command.
 * 
 * @deprecated The @ref joypad "Joypad Subsystem" now handles GameCube controller
 *             origins automatically. This structure is no longer needed.
 */
struct SI_origdat_gc {
    struct SI_condat_gc data;
    uint8_t deadzone0;
    uint8_t deadzone1;
};

/**
 * @brief SI controller data for all controller ports.
 * 
 * When reading N64 controller state, only the `c` member array will be populated.
 * When reading GC controller state, only the `gc` member array will be populated.
 * 
 * @deprecated The @ref joypad "Joypad Subsystem" now automatically normalizes
 *             Nintendo 64 and GameCube controller inputs. This structure is no
 *             longer needed. Use #joypad_inputs_t instead.
 */
typedef struct controller_data
{
    /** @brief Array of N64 controller state for each controller port. */
    struct SI_condat c[4];
    /** @brief Array of GameCube controller state for each controller port. */
    struct SI_condat_gc gc[4];
} SI_controllers_state_t;

/**
 * @brief SI GameCube controller origin data for all controller ports.
 * 
 * @deprecated The @ref joypad "Joypad Subsystem" now handles GameCube controller
 *             origins automatically. This structure is no longer needed.
 */
typedef struct controller_origin_data
{
    /** @brief Array of GameCube controller origin data for each controller port. */
    struct SI_origdat_gc gc[4];
} SI_controllers_origin_t;

/**************************************
 *  DEPRECATED FUNCTIONS 
 **************************************/

/// @cond

__attribute__((deprecated("use joybus_exec_cmd instead")))
void execute_raw_command(
    int controller,
    uint8_t command_id,
    size_t send_len,
    size_t recv_len,
    const void *send_data,
    void *recv_data
);

__attribute__((deprecated("use joypad_read_n64_inputs instead")))
void controller_read( struct controller_data * data );

__attribute__((deprecated("use joypad_get_inputs instead")))
void controller_read_gc( struct controller_data * data, const uint8_t rumble[4] );

__attribute__((deprecated("use joypad subsystem instead")))
void controller_read_gc_origin( struct controller_origin_data * data);

__attribute__((deprecated("use joypad_init instead")))
void controller_init( void );

__attribute__((deprecated("use joypad_poll instead")))
void controller_scan( void );

__attribute__((deprecated("use joypad_get_buttons_pressed instead")))
struct controller_data get_keys_down( void );

__attribute__((deprecated("use joypad_get_buttons_released instead")))
struct controller_data get_keys_up( void );

__attribute__((deprecated("use joypad_get_buttons_held instead")))
struct controller_data get_keys_held( void );

__attribute__((deprecated("use joypad_get_inputs instead")))
struct controller_data get_keys_pressed( void );

__attribute__((deprecated("use joypad_get_identifier instead")))
int get_controllers_present( void );

__attribute__((deprecated("use joypad_get_accessory_type instead")))
int get_accessories_present( struct controller_data * data );

__attribute__((deprecated("use joypad_get_accessory_type instead")))
int identify_accessory( int controller );

__attribute__((deprecated("use joypad_get_direction instead")))
int get_dpad_direction( int controller );

__attribute__((deprecated("use joybus_accessory_read instead")))
int read_mempak_address( int controller, uint16_t address, uint8_t *data );

__attribute__((deprecated("use joybus_accessory_write instead")))
int write_mempak_address( int controller, uint16_t address, uint8_t *data );

__attribute__((deprecated("use joypad_set_rumble_active instead")))
void rumble_start( int controller );

__attribute__((deprecated("use joypad_set_rumble_active instead")))
void rumble_stop( int controller );

/// @endcond

#ifdef __cplusplus
}
#endif

/** @} */ /* controller */

#endif
