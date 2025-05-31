#ifndef LIBDRAGON_TOOLS_POLYFILL_H
#define LIBDRAGON_TOOLS_POLYFILL_H

#ifdef __MINGW32__

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <process.h>
#include <share.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// if typedef doesn't exist (msvc, blah)
typedef intptr_t ssize_t;

/* Fetched from: https://stackoverflow.com/a/47229318 */
/* The original code is public domain -- Will Hartung 4/9/09 */
/* Modifications, public domain as well, by Antti Haapala, 11/10/17
   - Switched to getc on 5/23/19 */

ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    size_t pos;
    int c;

    if (lineptr == NULL || stream == NULL || n == NULL) {
        errno = EINVAL;
        return -1;
    }

    c = getc(stream);
    if (c == EOF) {
        return -1;
    }

    if (*lineptr == NULL) {
        *lineptr = (char*)malloc(128);
        if (*lineptr == NULL) {
            return -1;
        }
        *n = 128;
    }

    pos = 0;
    while(c != EOF) {
        if (pos + 1 >= *n) {
            size_t new_size = *n + (*n >> 2);
            if (new_size < 128) {
                new_size = 128;
            }
            char *new_ptr = (char*)realloc(*lineptr, new_size);
            if (new_ptr == NULL) {
                return -1;
            }
            *n = new_size;
            *lineptr = new_ptr;
        }

        ((unsigned char *)(*lineptr))[pos ++] = c;
        if (c == '\n') {
            break;
        }
        c = getc(stream);
    }

    (*lineptr)[pos] = '\0';
    return pos;
}

/* This function is original code in libdragon */
char *strndup(const char *s, size_t n)
{
  size_t len = strnlen(s, n);
  char *ret = (char*)malloc(len + 1);
  if (!ret) return NULL;
  memcpy(ret, s, len);
  ret[len] = '\0';
  return ret;
}

// tmpfile in mingw is broken (it uses msvcrt that tries to
// create a file in C:\, which is non-writable nowadays)
#define tmpfile()   mingw_tmpfile()

typedef void* HANDLE;
typedef const char* LPCSTR;
typedef int BOOL;
#define INVALID_HANDLE_VALUE ((HANDLE)(long)-1)
struct _SECURITY_ATTRIBUTES;

// Access rights
#define GENERIC_READ        0x80000000
#define GENERIC_WRITE       0x40000000

// Share modes
#define FILE_SHARE_READ     0x00000001
#define FILE_SHARE_WRITE    0x00000002
#define FILE_SHARE_DELETE   0x00000004

// Creation disposition
#define CREATE_NEW          1

// Flags and attributes
#define FILE_ATTRIBUTE_TEMPORARY     0x00000100
#define FILE_FLAG_DELETE_ON_CLOSE    0x04000000

__declspec(dllimport) HANDLE __stdcall CreateFileA(
    LPCSTR lpFileName,
    unsigned long dwDesiredAccess,
    unsigned long dwShareMode,
    struct _SECURITY_ATTRIBUTES* lpSecurityAttributes,
    unsigned long dwCreationDisposition,
    unsigned long dwFlagsAndAttributes,
    HANDLE hTemplateFile
);
__declspec(dllimport) int __stdcall CloseHandle(HANDLE);
__declspec(dllimport) unsigned long __stdcall GetLastError(void);
__declspec(dllimport) unsigned long __stdcall GetTickCount(void);

FILE *mingw_tmpfile(void) {
    static int counter = 0;
    char path[260];

    for (int i = 0; i < 4096; i++) {
        // Generate a random filename. Notice we *purposedly* not make this
        // very random with PID, timestamp, etc. because this is the third
        // iteration of mingw_tmpfile(): the previous ones were misbehaving
        // in various ways on various CRTs, Windows versions, etc. 
        // We want this code to be exercised often so that it is robust.
        snprintf(path, sizeof(path), "mksprite-%04x.tmp", counter++);

        HANDLE h = CreateFileA(
            path,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            CREATE_NEW,
            FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
            NULL
        );

        if (h != INVALID_HANDLE_VALUE) {
            int fd = _open_osfhandle((intptr_t)h, _O_RDWR | _O_BINARY);
            if (fd == -1) {
                CloseHandle(h);
                return NULL;
            }
            return fdopen(fd, "w+b");
        }

        // 80 = ERROR_FILE_EXISTS
        if (GetLastError() != 80)
            break;
    }

    return NULL;
}
char* strcasestr(const char* haystack, const char* needle)
{
    size_t needle_len = strlen(needle);
    size_t haystack_len = strlen(haystack);
    size_t i;

    if (needle_len > haystack_len)
        return NULL;

    for (i = 0; i <= haystack_len - needle_len; i++)
    {
        if (strncasecmp(haystack + i, needle, needle_len) == 0)
            return (char*)(haystack + i);
    }

    return NULL;
}

// Implementation from FreeBSD
void *memmem(const void *l, size_t l_len, const void *s, size_t s_len)
{
	char *cur, *last;
	const char *cl = (const char *)l;
	const char *cs = (const char *)s;

	/* we need something to compare */
	if (l_len == 0 || s_len == 0)
		return NULL;

	/* "s" must be smaller or equal to "l" */
	if (l_len < s_len)
		return NULL;

	/* special case where s_len == 1 */
	if (s_len == 1)
		return memchr(l, (int)*cs, l_len);

	/* the last position where its possible to find "s" in "l" */
	last = (char *)cl + l_len - s_len;

	for (cur = (char *)cl; cur <= last; cur++)
		if (cur[0] == cs[0] && memcmp(cur, cs, s_len) == 0)
			return cur;

	return NULL;
}

// rename() that ovewrites the destination file
#define rename(a,b)  mingw_rename(a,b)

#define MOVEFILE_REPLACE_EXISTING 0x00000001
#define MOVEFILE_WRITE_THROUGH    0x00000008

__declspec(dllimport) int __stdcall MoveFileExA(const char *lpExistingFileName,
                                                const char *lpNewFileName,
                                                unsigned long dwFlags);
__declspec(dllimport) unsigned long __stdcall GetLastError(void);

static void map_windows_error_to_errno(unsigned long err) {
    switch (err) {
        case 2:    errno = ENOENT; break;        // ERROR_FILE_NOT_FOUND
        case 3:    errno = ENOENT; break;        // ERROR_PATH_NOT_FOUND
        case 5:    errno = EACCES; break;        // ERROR_ACCESS_DENIED
        case 32:   errno = EBUSY; break;         // ERROR_SHARING_VIOLATION
        case 80:   errno = EEXIST; break;        // ERROR_FILE_EXISTS
        case 183:  errno = EEXIST; break;        // ERROR_ALREADY_EXISTS
        default:   errno = EIO; break;           // Generic error
    }
}

int mingw_rename(const char *oldpath, const char *newpath) {
    if (MoveFileExA(oldpath, newpath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return 0;
    } else {
        map_windows_error_to_errno(GetLastError());
        return -1;
    }
}

// POISX mkdir has a mode argument, but mingw's mkdir doesn't
#define mkdir(path, mode) mkdir(path)

#ifdef __cplusplus
}
#endif


#endif

#endif
