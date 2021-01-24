/**
 * @file do_ctors.c
 * @brief C++ constructor handling
 * @ingroup system
 */
#include <stdint.h>

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
	func_ptr * ctor_addr = &__CTOR_END__ - 1;
	func_ptr * ctor_sentinel = &__CTOR_LIST__;
	while (ctor_addr > ctor_sentinel) {
		if (*ctor_addr) (*ctor_addr)();
		ctor_addr--;
	}
}

/** @} */
