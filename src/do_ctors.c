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

/** @brief Pointer to the size of the constructor list */
extern uint32_t __CTOR_LIST_SIZE__  __attribute__((section (".data")));
/** @brief Pointer to the beginning of the constructor list */
extern func_ptr __CTOR_LIST__[];
/** @brief Pointer to the end of the constructor list */
extern func_ptr __CTOR_END__[];

/** 
 * @brief Execute global constructors
 */
void __do_global_ctors() 
{
	unsigned int tot_constructors = __CTOR_LIST_SIZE__;
	for (void (**f)(void) = (void (**)(void))(__CTOR_LIST__); tot_constructors > 0; tot_constructors--, f++)
		(**f)();
}

/** @} */
