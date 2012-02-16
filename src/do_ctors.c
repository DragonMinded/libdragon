/**
 * @file do_ctors.c
 * @brief C++ constructor handling
 * @ingroup system
 */

/**
 * @addtogroup system
 * @{
 */

/** @brief Function pointer */
typedef void (*func_ptr)(void);

/** @brief Pointer to the beginning of the constructor list */
extern func_ptr __CTOR_LIST__[];
/** @brief Pointer to the end of the constructor list */
extern func_ptr __CTOR_END__[];

/** 
 * @brief Execute global constructors
 */
void __do_global_ctors() 
{
	unsigned int tot_constructors = *(unsigned int*)(__CTOR_LIST__);
	for (void (**f)(void) = (void (**)(void))(__CTOR_LIST__ + 1); tot_constructors > 0; tot_constructors--, f++)
		(**f)();
}

/** @} */
