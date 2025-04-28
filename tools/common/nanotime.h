#include <stdint.h>

#ifdef _WIN32
#include <stdint.h>

typedef struct {
    uint32_t dwLowDateTime;
    uint32_t dwHighDateTime;
} MY_FILETIME;

extern "C" __declspec(dllimport) void __stdcall GetSystemTimeAsFileTime(MY_FILETIME *lpFileTime);

uint64_t nanotime(void) {
    MY_FILETIME ft;
    GetSystemTimeAsFileTime(&ft);

    uint64_t high = (uint64_t)ft.dwHighDateTime;
    uint64_t low = (uint64_t)ft.dwLowDateTime;
    uint64_t filetime_100ns = (high << 32) | low;

    return (filetime_100ns - 116444736000000000ULL) * 100;
}

#else
#include <time.h>

uint64_t nanotime(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}
#endif
