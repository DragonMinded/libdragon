/**
 * @file utils.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Misc utilities functions and macros
 */
#ifndef __LIBDRAGON_UTILS_H
#define __LIBDRAGON_UTILS_H

#include <string.h>  // memcpy

/** @brief Swap two values */
#define SWAP(a, b) ({ typeof(a) t = a; a = b; b = t; })

/** @brief Return the maximum of two values */
#define MAX(a,b)  ({ typeof(a) _a = a; typeof(b) _b = b; _a > _b ? _a : _b; })
/** @brief Return the minimum of two values */
#define MIN(a,b)  ({ typeof(a) _a = a; typeof(b) _b = b; _a < _b ? _a : _b; })
/** @brief Clamp a value between min and max */
#define CLAMP(x, min, max) (MIN(MAX((x), (min)), (max)))

/** @brief Round n up to the next multiple of d */
#define ROUND_UP(n, d) ({ \
	typeof(n) _n = n; typeof(d) _d = d; \
	(((_n) + (_d) - 1) / (_d) * (_d)); \
})

/** @brief Round n down to the previous multiple of d */
#define ROUND_DOWN(n, d) ({ \
    typeof(n) _n = n; typeof(d) _d = d; \
    ((_n >= 0) ? ((_n) / (_d) * (_d)) : -(((-_n + (_d) - 1) / (_d)) * (_d))); \
})

/** @brief Return the ceil of n/d */
#define DIVIDE_CEIL(n, d) ({ \
	typeof(n) _n = n; typeof(d) _d = d; \
	((_n) + (_d) - 1) / (_d); \
})

/**	@brief Absolute number */
#define ABS(x) ({ \
	typeof(x) _x = x; \
	(_x < 0 ? -_x : _x); \
})

/** @brief Type-safe bitcast from float to integer */
#define F2I(f)   ({ uint32_t __i; memcpy(&__i, &(f), 4); __i; })

/** @brief Type-safe bitcast from integer to float */
#define I2F(i)   ({ float __f; memcpy(&__f, &(i), 4); __f; })

/** @brief Hint for the compiler that the condition is likely to happen */
#define LIKELY(cond)  	__builtin_expect(!!(cond), 1)
/** @brief Hint for the compiler that the condition is unlikely to happen */
#define UNLIKELY(cond)  __builtin_expect(!!(cond), 0)

/** @brief UTF-8 decoding */
uint32_t __utf8_decode(const char **str);

#endif
