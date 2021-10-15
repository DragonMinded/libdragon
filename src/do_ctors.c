/**
 * @file do_ctors.c
 * @brief C++ constructor handling
 * @ingroup system
 */
#include <stdint.h>
#include <assert.h>
#include <debug.h>

/**
 * @addtogroup system
 * @{
 */

/** @brief Function pointer */
typedef void (*func_ptr)(void);

/** @brief Pointer to the beginning of the constructor list */
extern func_ptr __CTOR_LIST__ __attribute__((section (".data")));
/** @brief Pointer to the end of the constructor list */
extern func_ptr __CTOR_END__ __attribute__((section (".data")));

/**
 * @brief Execute global constructors
 * "Constructors are called in reverse order of the list"
 * @see https://gcc.gnu.org/onlinedocs/gccint/Initialization.html
 */
void __do_global_ctors()
{
	// Somehow __CTOR_END__ points to the next location after the list and the
	// last item is a zero, so start from the actual end.
	func_ptr * ctor_addr = &__CTOR_END__ - 2;
	func_ptr * ctor_sentinel = &__CTOR_LIST__;
	// This will break if you link using LD. You'll need to change the linker
	// script and add the sentinel manually. g++ already does that but ld does
	// not. In that case, this will skip the last function. If this was an
	// inclusive loop, it would fail for g++ as the last item won't be a valid
	// pointer. Also see __CTOR_LIST__ in n64.ld
	assertf(
		(uint32_t)*ctor_sentinel == 0xFFFFFFFF && (uint32_t)*(&__CTOR_END__ - 1) == 0x0,
		"Invalid sentinel, ensure you link via g++"
	);
	while (ctor_addr > ctor_sentinel) {
		assert(*ctor_addr != NULL);
		(*ctor_addr)();
		ctor_addr--;
	}
}

/** @} */
