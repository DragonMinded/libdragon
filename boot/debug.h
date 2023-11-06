#ifndef BOOT_DEBUG_H
#define BOOT_DEBUG_H

#define DEBUG 0

#if DEBUG

#include "pputils.h"

void usb_init(void);
void _usb_print(int ssize, const char *string, int nargs, ...);

#define debugf(s, ...)   _usb_print(__builtin_strlen(s), s "    ", __COUNT_VARARGS(__VA_ARGS__), ##__VA_ARGS__)

#else 

static inline void usb_init(void) {}
#define debugf(s, ...) ({ })

#endif

#endif