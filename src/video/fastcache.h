#ifndef FASTCACHE_H
#define FASTCACHE_H

// Fast cache flush/invalidate ops.

// This is similar to libdragon's functions, but it generates very fast
// inlined code, with a noticeable performance difference especially
// when the size of the data to flush is known at compile-time.

// Notice that, in general, these functions try to be cautious, assume
// unaligned addresses, and make sure to touch enough cachelines to honor
// the request under all circumstances. For instance, when asking to flush
// 2 bytes, 2 cachelines will be flushed, because in theory the two bytes could
// sit in two different cachelines. 

#define cache_op(op, addr_, length_) ({ \
    void *addr=(void*)(((unsigned long)(addr_)&(~3))); \
    int length = (length_)+16; \
    for (;length>64;length-=64,addr+=64) { \
		asm ("\tcache %0,0(%1)\n"::"i" (op), "r" (addr)); \
		asm ("\tcache %0,16(%1)\n"::"i" (op), "r" (addr)); \
		asm ("\tcache %0,32(%1)\n"::"i" (op), "r" (addr)); \
		asm ("\tcache %0,48(%1)\n"::"i" (op), "r" (addr)); \
	} \
	/* In case length is know at compile-time and there are no more than 4 */ \
	/* cachelines to flush, generate manually-unrolled cache instructions. */ \
	if (__builtin_constant_p(length_)) { \
		if (length > 0 ) asm ("\tcache %0, 0(%1)\n"::"i" (op), "r" (addr)); \
		if (length > 16) asm ("\tcache %0,16(%1)\n"::"i" (op), "r" (addr)); \
		if (length > 32) asm ("\tcache %0,32(%1)\n"::"i" (op), "r" (addr)); \
		if (length > 48) asm ("\tcache %0,48(%1)\n"::"i" (op), "r" (addr)); \
	} else { \
	    for (;length>=0;length-=16,addr+=16) { \
			asm ("\tcache %0,0(%1)\n"::"i" (op), "r" (addr)); \
		} \
	} \
})

#define fast_data_cache_hit_writeback(mem, len) ({ \
	cache_op(0x19, mem, len); \
})

#define fast_data_cache_hit_writeback_invalidate(mem, len) ({ \
	cache_op(0x15, mem, len); \
})

#define fast_data_cache_hit_invalidate(mem, len) ({ \
	cache_op(0x11, mem, len); \
})

#endif
