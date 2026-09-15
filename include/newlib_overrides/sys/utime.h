/**
 * @file utime.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef _SYS_UTIME_H
#define _SYS_UTIME_H

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Structure for file access and modification times */
struct utimbuf
{
  time_t actime;
  time_t modtime;
};

int utime(const char *path, const struct utimbuf *times);

#ifdef __cplusplus
};
#endif

#endif /* _SYS_UTIME_H */
