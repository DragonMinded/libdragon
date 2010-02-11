#ifndef __LIBDRAGON_INTERRUPT_H
#define __LIBDRAGON_INTERRUPT_H

void register_AI_handler( void (*callback)() );
void register_VI_handler( void (*callback)() );
void register_PI_handler( void (*callback)() );
void register_DP_handler( void (*callback)() );
void set_AI_interrupt( int active);
void set_VI_interrupt( int active, unsigned long line );
void set_PI_interrupt( int active );
void set_DP_interrupt( int active );
void set_MI_interrupt( int active, int clear );

#endif
