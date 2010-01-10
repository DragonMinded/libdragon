#ifndef __LIBDRAGON_CONTROLLER_H
#define __LIBDRAGON_CONTROLLER_H

/* Bridge from libnds to libdragon calling standard */
#define controller_read controller_Read

/* Initialize the controller system. */
void controller_init();

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
   not pressed. */
int get_dpad_direction( int controller );

#endif
