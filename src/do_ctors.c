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
extern func_ptr __CTOR_LIST__[] __attribute__((section (".data")));
/** @brief Pointer to the end of the constructor list */
extern func_ptr __CTOR_END__[] __attribute__((section (".data")));

/** @brief Pointer to the beginning of exception frames */
extern char __EH_FRAME_BEGIN__[];

/**
 * @brief Data used for registering exception frame info
 * Must be 24 bytes big and 4-byte aligned.
 */
static uint32_t eh_frame_object[6];

/**
 * @brief Register exception frames
 * This is used as a placeholder if the user does not link in libgcc
 */
void __attribute__((weak)) __register_frame_info(void *begin, uint32_t *ob)
{
	
}

/**
 * @brief Execute global constructors
 * "Constructors are called in reverse order of the list"
 * @see https://gcc.gnu.org/onlinedocs/gccint/Initialization.html
 *
 * This version of the function is kept for compatibility for projects not using
 * the build system but linking directly with ld in a legacy setup. For the
 * modern version see #__wrap___do_global_ctors which is activated by the new
 * build system (n64.mk) via --wrap linker flag. Do not use that flag if you are
 * using ld so that this function is used instead.
 */
void __do_global_ctors()
{
	func_ptr * ctor_addr = __CTOR_END__ - 1;
	func_ptr * ctor_sentinel = __CTOR_LIST__;
	// Register exception handler frames
	__register_frame_info(__EH_FRAME_BEGIN__, eh_frame_object);
	assertf((uint32_t) ctor_sentinel[0] != 0xFFFFFFFF,
		"Invalid constructor sentinel.\nWhen linking with g++, please specify:\n   --wrap __do_global_ctors");
	while (ctor_addr >= ctor_sentinel) {
		if (*ctor_addr) (*ctor_addr)();
		ctor_addr--;
	}
}

/**
 * @brief Execute global constructors
 * This version is used by the new build system (n64.mk) via the --wrap linker
 * flag. When that is provided, this version will be utilized instead. New build
 * system always links with g++ which is not directly compatible with ld when it
 * comes to constructors and enables that flag by default.
 */
void __wrap___do_global_ctors()
{
	// g++ seem to insert a rogue __CTOR_END__ symbol that shifts it to the next
	// 4 bytes but weirdly disassembly shows it in correct place. __CTOR_END__ - 1
	// is the actual value and we subtract one more to skip the zero value and
	// thus the "-2".
	func_ptr * ctor_addr = __CTOR_END__ - 2;
	func_ptr * ctor_sentinel = __CTOR_LIST__;
	// Register exception handler frames
	__register_frame_info(__EH_FRAME_BEGIN__, eh_frame_object);
	// This will break if you link using LD. You'll need to change the linker
	// script and add the sentinel manually. g++ already does that but ld does
	// not. In that case, this will skip the last function. If this was an
	// inclusive loop, it would fail for g++ as the last item won't be a valid
	// pointer. Also see __CTOR_LIST__ in n64.ld and #__do_global_ctors
	assertf(
		(uint32_t) ctor_sentinel[0] == 0xFFFFFFFF &&
		(uint32_t) ctor_sentinel[(__CTOR_END__ - __CTOR_LIST__) - 1] == 0x0,
		"Invalid sentinel, ensure you link via g++"
	);
	while (ctor_addr > ctor_sentinel) {
		assert(*ctor_addr != NULL);
		(*ctor_addr)();
		ctor_addr--;
	}
}

/** @} */
