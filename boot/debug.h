#ifndef BOOT_DEBUG_H
#define BOOT_DEBUG_H

#define DEBUG       1   // 0: disabled, 1:n64, 2:ique

#if DEBUG

#include "pputils.h"

__attribute__((far))
void usb_init(void);

__attribute__((far))
void _usb_print(int ssize, const char *string, int nargs, ...);

#define debugf(s, ...)   _usb_print(__builtin_strlen(s), s "    ", __COUNT_VARARGS(__VA_ARGS__), ##__VA_ARGS__)

#else 

static inline void usb_init(void) {}
#define debugf(s, ...) ({ })

#endif

#endif