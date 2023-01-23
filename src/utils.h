#ifndef __LIBDRAGON_UTILS_H
#define __LIBDRAGON_UTILS_H

#include <string.h>  // memcpy

/**
 * Misc utilities functions and macros. Internal header.
 */

#define SWAP(a, b) ({ typeof(a) t = a; a = b; b = t; })

#define MAX(a,b)  ({ typeof(a) _a = a; typeof(b) _b = b; _a > _b ? _a : _b; })
#define MIN(a,b)  ({ typeof(a) _a = a; typeof(b) _b = b; _a < _b ? _a : _b; })
#define CLAMP(x, min, max) (MIN(MAX((x), (min)), (max)))

/** @brief Round n up to the next multiple of d */
#define ROUND_UP(n, d) ({ \
	typeof(n) _n = n; typeof(d) _d = d; \
	(((_n) + (_d) - 1) / (_d) * (_d)); \
})

/** @brief Return the ceil of n/d */
#define DIVIDE_CEIL(n, d) ({ \
	typeof(n) _n = n; typeof(d) _d = d; \
	((_n) + (_d) - 1) / (_d); \
})

/**	@brief Absolute number */
#define ABS(x) ({ \
	typedef(x) _x = x; \
	(_x < 0 ? -_x : _x); \
})

/** @brief Type-safe bitcast from float to integer */
#define F2I(f)   ({ uint32_t __i; memcpy(&__i, &(f), 4); __i; })

/** @brief Type-safe bitcast from integer to float */
#define I2F(i)   ({ float __f; memcpy(&__f, &(i), 4); __f; })

#endif
