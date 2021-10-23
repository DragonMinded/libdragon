#ifndef __LIBDRAGON_UTILS_H
#define __LIBDRAGON_UTILS_H

/**
 * Misc utilities functions and macros. Internal header.
 */

#define MAX(a,b)  ({ typeof(a) _a = a; typeof(b) _b = b; _a > _b ? _a : _b; })
#define MIN(a,b)  ({ typeof(a) _a = a; typeof(b) _b = b; _a < _b ? _a : _b; })

/** Round n up to the next multiple of d */
#define ROUND_UP(n, d) ({ \
	typeof(n) _n = n; typeof(d) _d = d; \
	(((_n) + (_d) - 1) / (_d) * (_d)); \
})

/** Return the ceil of n/d */
#define DIVIDE_CEIL(n, d) ({ \
	typeof(n) _n = n; typeof(d) _d = d; \
	((_n) + (_d) - 1) / (_d); \
})


#endif
