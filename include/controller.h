#ifndef __LIBDRAGON_CONTROLLER_H
#define __LIBDRAGON_CONTROLLER_H

typedef struct SI_condat 
{
    unsigned : 16;
    unsigned err : 2; // error status
    unsigned : 14;

    union 
    {
        struct 
        {
            unsigned buttons : 16;
            unsigned : 16;
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

/* Initialize the controller system. */
void controller_init();

/* Read the controller status immediately and return results to data.  If
   calling this function, one should not also call controller_scan */
void controller_read( struct controller_data * data);

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
