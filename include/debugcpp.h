/**
 * @file debug.h
 * @brief Debugging Support (C++)
 */

#ifndef __LIBDRAGON_DEBUGCPP_H
#define __LIBDRAGON_DEBUGCPP_H

#if defined(__cplusplus) && !defined(NDEBUG)
    // We need to run some initialization code only in case libdragon is compiled from
    // a C++ program. So we hook a few common initialization functions and run our code.
    // C programs are not affected and the C++-related code will be unused and stripped by the linker.
    ///@cond
    void __debug_init_cpp(void);

    #define console_init() ({ __debug_init_cpp(); console_init(); })
    #define dfs_init(a) ({ __debug_init_cpp(); dfs_init(a);})
    #define joypad_init() ({ __debug_init_cpp(); joypad_init(); })
    #define timer_init() ({ __debug_init_cpp(); timer_init(); })
    #define display_init(a,b,c,d,e) ({ __debug_init_cpp(); display_init(a,b,c,d,e); })
    #define debug_init_isviewer() ({ __debug_init_cpp(); debug_init_isviewer(); })
    #define debug_init_usblog() ({ __debug_init_cpp(); debug_init_usblog(); })
    ///@endcond
#endif

#endif
