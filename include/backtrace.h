#ifndef __LIBDRAGON_BACKTRACE_H
#define __LIBDRAGON_BACKTRACE_H

int backtrace(void **buffer, int size);
char** backtrace_symbols(void **buffer, int size);

#endif
