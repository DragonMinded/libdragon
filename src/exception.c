/**
 * @file exception.c
 * @brief Exception Handler
 * @ingroup exceptions
 */
#include "exception.h"
#include "exception_internal.h"
#include "console.h"
#include "n64sys.h"
#include "debug.h"
#include "regsinternal.h"
#include "kernel/kernel_internal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

/** Invalid TLS Minimum Address */
#define TLS_INVALID_MIN (uint32_t)(KERNEL_TP_INVALID-TP_OFFSET)
/**
 * @brief Syscall exception handler entry
 */
typedef struct {
	/** @brief Exception handler */
	syscall_handler_t handler;
	/** @brief Syscall code range start */
	uint32_t first_code;
	/** @brief Syscall code range end */
	uint32_t last_code;
} syscall_handler_entry_t;

/** @brief Maximum number of syscall handlers that can be registered. */
#define MAX_SYSCALL_HANDLERS 4

/** @brief Unhandled exception handler currently registered with exception system */
static void (*__exception_handler)(exception_t*) = exception_default_handler;
/** @brief Base register offset as defined by the interrupt controller */
extern volatile reg_block_t __baseRegAddr;
/** @brief Syscall exception handlers */
static syscall_handler_entry_t __syscall_handlers[MAX_SYSCALL_HANDLERS];

exception_handler_t register_exception_handler( exception_handler_t cb )
{
	exception_handler_t old = __exception_handler;
	__exception_handler = cb;
	return old;
}


/** 
 * @brief Dump a brief recap of the exception.
 * 
 * @param[in] out File to write to
 * @param[in] ex Exception to dump
 */
void __exception_dump_header(FILE *out, exception_t* ex) {
	uint32_t cr = ex->regs->cr;
	uint32_t fcr31 = ex->regs->fc31;
	uint32_t epc = (uint32_t)(ex->regs->epc + ((cr & C0_CAUSE_BD) ? 4 : 0));

	fprintf(out, "%s exception at PC:%08lX\n", ex->info, epc);
	switch (ex->code) {
		case EXCEPTION_CODE_STORE_ADDRESS_ERROR:
		case EXCEPTION_CODE_LOAD_I_ADDRESS_ERROR:
		case EXCEPTION_CODE_TLB_STORE_MISS:
		case EXCEPTION_CODE_TLB_LOAD_I_MISS:
		case EXCEPTION_CODE_TLB_MODIFICATION: {
			uint64_t badvaddr = C0_BADVADDR();
			if ((uint64_t)(int32_t)badvaddr == badvaddr)
				fprintf(out, "Exception address: %08lX\n", (uint32_t)badvaddr);
			else
				fprintf(out, "Exception address: %016llX\n", badvaddr);
			break;
		}

		case EXCEPTION_CODE_FLOATING_POINT: {
			const char *space = "";
			fprintf(out, "FPU status: %08lX [", C1_FCR31());
			if (fcr31 & C1_CAUSE_INEXACT_OP) fprintf(out, "%sINEXACT", space), space=" ";
			if (fcr31 & C1_CAUSE_OVERFLOW) fprintf(out, "%sOVERFLOW", space), space=" ";
			if (fcr31 & C1_CAUSE_UNDERFLOW) fprintf(out, "%sUNDERFLOW", space), space=" ";
			if (fcr31 & C1_CAUSE_DIV_BY_0) fprintf(out, "%sDIV0", space), space=" ";
			if (fcr31 & C1_CAUSE_INVALID_OP) fprintf(out, "%sINVALID", space), space=" ";
			if (fcr31 & C1_CAUSE_NOT_IMPLEMENTED) fprintf(out, "%sNOTIMPL", space), space=" ";
			fprintf(out, "]\n");
			break;
		}

		case EXCEPTION_CODE_COPROCESSOR_UNUSABLE:
			fprintf(out, "COP: %ld\n", C0_GET_CAUSE_CE(cr));
			break;

		case EXCEPTION_CODE_WATCH:
			fprintf(out, "Watched address: %08lX\n", C0_WATCHLO() & ~3);
			break;

		case EXCEPTION_CODE_SYS_CALL:
			fprintf(out, "Syscall code: %05lX\n", (*(uint32_t*)ex->regs->epc >> 6) & 0xfffff);
			break;

		case EXCEPTION_CODE_D_BUS_ERROR: {
			uint32_t opcode = *(uint32_t*)epc;
			uint64_t base = ex->regs->gpr[((opcode >> 21) & 0x1F)];
			uint64_t offset = (int16_t)(opcode & 0xFFFF);
			uint64_t badvaddr = base + offset;
			if ((uint64_t)(int32_t)badvaddr == badvaddr)
				fprintf(out, "Exception address: %08lX\n", (uint32_t)badvaddr);
			else
				fprintf(out, "Exception address: %016llX\n", badvaddr);
		}	break;

		case EXCEPTION_CODE_I_BUS_ERROR:
		default:
			break;
	}
}

/**
 * @brief Helper to dump the GPRs of an exception
 * 
 * @param ex 		Exception
 * @param cb 		Callback that will be called for each register
 * @param arg 		Argument to pass to the callback
 */
void __exception_dump_gpr(exception_t* ex, void (*cb)(void *arg, const char *regname, char* value), void *arg) {
	char buf[24];
	for (int i=0;i<34;i++) {
		uint64_t v = (i<32) ? ex->regs->gpr[i] : (i == 33) ? ex->regs->lo : ex->regs->hi;
		if ((int32_t)v == v) {
			snprintf(buf, sizeof(buf), "---- ---- %04llx %04llx", (v >> 16) & 0xFFFF, v & 0xFFFF);
		} else {
			snprintf(buf, sizeof(buf), "%04llx %04llx %04llx %04llx", v >> 48, (v >> 32) & 0xFFFF, (v >> 16) & 0xFFFF, v & 0xFFFF);
		}
		cb(arg, __mips_gpr[i], buf);
	}
}

/**
 * @brief Helper to dump the FPRs of an exception
 * 
 * @param ex 		Exception
 * @param cb 		Callback that will be called for each register
 * @param arg 		Argument to pass to the callback
 */
// Make sure that -ffinite-math-only is disabled otherwise the compiler will assume that no NaN/Inf can exist
// and thus __builtin_isnan/__builtin_isinf are folded to false at compile-time.
__attribute__((optimize("no-finite-math-only"), noinline))
void __exception_dump_fpr(exception_t* ex, void (*cb)(void *arg, const char *regname, char* hexvalue, char *singlevalue, char *doublevalue), void *arg) {
	char hex[32], single[32], doubl[32]; char *singlep, *doublep;
	for (int i = 0; i<32; i++) {
		uint64_t fpr64 = ex->regs->fpr[i];
		uint32_t fpr32 = fpr64;

		snprintf(hex, sizeof(hex), "%016llx", fpr64);

		float f;  memcpy(&f, &fpr32, sizeof(float));
		double g; memcpy(&g, &fpr64, sizeof(double));

		// Check for denormal on the integer representation. Unfortunately, even
		// fpclassify() generates an unmaskable exception on denormals, so it can't be used.
		// Open GCC bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66462
		if ((fpr32 & 0x7F800000) == 0 && (fpr32 & 0x007FFFFF) != 0)
			singlep = "<Denormal>";
		else if ((fpr32 & 0x7F800000) == 0x7F800000 && (fpr32 & 0x007FFFFF) != 0)
			singlep = "<NaN>";
		else if (__builtin_isinf(f))
			singlep = (f < 0) ? "<-Inf>" : "<+Inf>";
		else
			sprintf(single, "%.12g", f), singlep = single;

		if ((fpr64 & 0x7FF0000000000000ull) == 0 && (fpr64 & 0x000FFFFFFFFFFFFFull) != 0)
			doublep = "<Denormal>";
		else if ((fpr64 & 0x7FF0000000000000ull) == 0x7FF0000000000000ull && (fpr64 & 0x000FFFFFFFFFFFFFull) != 0)
			doublep = "<NaN>";
		else if (__builtin_isinf(g))
			doublep = (g < 0) ? "<-Inf>" : "<+Inf>";
		else
			sprintf(doubl, "%.17g", g), doublep = doubl;

		cb(arg, __mips_fpreg[i], hex, singlep, doublep);
	}
}

#ifndef NDEBUG
static void debug_exception(exception_t* ex) {
	debugf("\n\n******* CPU EXCEPTION *******\n");
	__exception_dump_header(stderr, ex);

	if (true) {
		int idx = 0;
		void cb(void *arg, const char *regname, char* value) {
			debugf("%s: %s%s", regname, value, ++idx % 4 ? "   " : "\n");
		}
		debugf("GPR:\n"); 
		__exception_dump_gpr(ex, cb, NULL);
		debugf("\n\n");
	}	
	
	if (ex->code == EXCEPTION_CODE_FLOATING_POINT) {
		void cb(void *arg, const char *regname, char* hex, char *singlep, char *doublep) {
			debugf("%4s: %s (%16s | %22s)\n", regname, hex, singlep, doublep);
		}
		debugf("FPR:\n"); 
		__exception_dump_fpr(ex, cb, NULL);
		debugf("\n");
	}
}
#endif

void exception_default_handler(exception_t* ex) {
	#ifndef NDEBUG
	static bool backtrace_exception = false;

	// Write immediately as much data as we can to the debug spew. This is the
	// "safe" path, because it doesn't involve touching the console drawing code.
	debug_exception(ex);

	// Show a backtrace (starting from just before the exception handler)
	// Avoid recursive exceptions during backtrace printing
	if (backtrace_exception) abort();
	backtrace_exception = true;
	extern void __debug_backtrace(FILE *out, bool skip_exception);
	__debug_backtrace(stderr, true);
	backtrace_exception = false;

	// Run the inspector
	__inspector_exception(ex);
	#endif

	abort();
}

/**
 * @brief Return true if the specified address is unmapped in 64-bit kernel space
 */
static bool is_unmapped_kx64(uint64_t vaddr)
{
  if (vaddr <= 0x000000ffffffffffull) return false;
  if (vaddr <= 0x3fffffffffffffffull) return true;
  if (vaddr <= 0x400000ffffffffffull) return false;
  if (vaddr <= 0x7fffffffffffffffull) return true;
  if (vaddr <= 0x80000000ffffffffull) return false;
  if (vaddr <= 0x87ffffffffffffffull) return true;
  if (vaddr <= 0x88000000ffffffffull) return false;
  if (vaddr <= 0x8fffffffffffffffull) return true;
  if (vaddr <= 0x90000000ffffffffull) return false;
  if (vaddr <= 0x97ffffffffffffffull) return true;
  if (vaddr <= 0x98000000ffffffffull) return false;
  if (vaddr <= 0x9fffffffffffffffull) return true;
  if (vaddr <= 0xa0000000ffffffffull) return false;
  if (vaddr <= 0xa7ffffffffffffffull) return true;
  if (vaddr <= 0xa8000000ffffffffull) return false;
  if (vaddr <= 0xafffffffffffffffull) return true;
  if (vaddr <= 0xb0000000ffffffffull) return false;
  if (vaddr <= 0xb7ffffffffffffffull) return true;
  if (vaddr <= 0xb8000000ffffffffull) return false;
  if (vaddr <= 0xbfffffffffffffffull) return true;
  if (vaddr <= 0xc00000ff7fffffffull) return false;
  if (vaddr <= 0xffffffff7fffffffull) return true;
  if (vaddr <= 0xffffffff9fffffffull) return false;
  if (vaddr <= 0xffffffffbfffffffull) return false;
  if (vaddr <= 0xffffffffdfffffffull) return false;
  if (vaddr <= 0xffffffffffffffffull) return false;
  __builtin_unreachable();
}

/**
 * @brief Fetch the string name of the exception
 *
 * @param[in] cr
 *            Cause register's value
 *
 * @return String representation of the exception
 */
static const char* __get_exception_name(exception_t *ex)
{
	static const char* exceptionMap[] =
	{
		"Interrupt",								// 0
		"TLB Modification",							// 1
		"TLB Miss (load/instruction fetch)",		// 2
		"TLB Miss (store)",							// 3
		"Address Error (load/instruction fetch)",	// 4
		"Address Error (store)",					// 5
		"Bus Error (instruction fetch)",			// 6
		"Bus Error (load/store)",					// 7
		"Syscall",									// 8
		"Breakpoint",								// 9
		"Reserved Instruction",						// 10
		"Coprocessor Unusable",						// 11
		"Arithmetic Overflow",						// 12
		"Trap",										// 13
		"Reserved",									// 13
		"Floating-Point",							// 15
		"Reserved",									// 16
		"Reserved",									// 17
		"Reserved",									// 18
		"Reserved",									// 19
		"Reserved",									// 20
		"Reserved",									// 21
		"Reserved",									// 22
		"Watch",									// 23
		"Reserved",									// 24
		"Reserved",									// 25
		"Reserved",									// 26
		"Reserved",									// 27
		"Reserved",									// 28
		"Reserved",									// 29
		"Reserved",									// 30
		"Reserved",									// 31
	};


	// When possible, by peeking into the exception state and COP0 registers
	// we can provide a more detailed exception name.
	uint32_t epc = ex->regs->epc + (ex->regs->cr & C0_CAUSE_BD ? 4 : 0);
	uint64_t badvaddr = C0_BADVADDR();

	switch (ex->code) {
	case EXCEPTION_CODE_FLOATING_POINT:
		if (ex->regs->fc31 & C1_CAUSE_DIV_BY_0) {
			return "Floating point divide by zero";
		} else if (ex->regs->fc31 & C1_CAUSE_INVALID_OP) {
			return "Floating point invalid operation";
		} else if (ex->regs->fc31 & C1_CAUSE_OVERFLOW) {
			return "Floating point overflow";
		} else if (ex->regs->fc31 & C1_CAUSE_UNDERFLOW) {
			return "Floating point underflow";
		} else if (ex->regs->fc31 & C1_CAUSE_INEXACT_OP) {
			return "Floating point inexact operation";
		} else {
			return "Generic floating point";
		}
	case EXCEPTION_CODE_TLB_LOAD_I_MISS:
		if (epc == (uint32_t)badvaddr) {
			return "Invalid program counter address";
		} else if (badvaddr < 128) {
			// This is probably a NULL pointer dereference, though it can go through a structure or an array,
			// so leave some margin to the actual faulting address.
			return "NULL pointer dereference (read)";
		} else {
			if(badvaddr >= TLS_INVALID_MIN && badvaddr < (TLS_INVALID_MIN+TLS_SIZE)) {
				if(__kernel) {
					return "Read from TLS in interrupt handler";
				} else {
					return "Cannot access TLS without kernel_init()";
				}
				
			} else {
				return "Read from invalid memory address";
			}
		}
	case EXCEPTION_CODE_TLB_STORE_MISS:
		if (badvaddr < 128) {
			return "NULL pointer dereference (write)";
		} else {
			if(badvaddr >= TLS_INVALID_MIN && badvaddr < (TLS_INVALID_MIN+TLS_SIZE)) {
				if(__kernel) {
					return "Write to TLS in interrupt handler";
				} else {
					return "Cannot access TLS without kernel_init()";
				}
			} else {
				return "Write to invalid memory address";
			}
		}
	case EXCEPTION_CODE_TLB_MODIFICATION:
		return "Write to read-only memory";
	case EXCEPTION_CODE_LOAD_I_ADDRESS_ERROR:
		if (epc == (uint32_t)badvaddr) {
			if (is_unmapped_kx64(badvaddr))
				return "Program counter in invalid 64-bit address";
			else
				return "Misaligned program counter address";
		} else {
			if (is_unmapped_kx64(badvaddr))
				return "Read from invalid 64-bit address";
			else {
				if(badvaddr >= TLS_INVALID_MIN && badvaddr < (TLS_INVALID_MIN+TLS_SIZE)) {
					if(__kernel) {
						return "Read from TLS in interrupt handler";
					} else {
						return "Cannot access TLS without kernel_init()";
					}
				} else {
					return "Misaligned read from memory";
				}
			}
		}
	case EXCEPTION_CODE_STORE_ADDRESS_ERROR:
		if(badvaddr >= TLS_INVALID_MIN && badvaddr < (TLS_INVALID_MIN+TLS_SIZE)) {
			if(__kernel) {
				return "Write to TLS in interrupt handler";
			} else {
				return "Cannot access TLS without kernel_init()";
			}
		} else {
			return "Misaligned write to memory";
		}
	case EXCEPTION_CODE_SYS_CALL:
		return "Unhandled syscall";
	case EXCEPTION_CODE_TRAP: {
		// GCC MIPS generate a trap with code 7 to guard divisions
		uint32_t code = (*(uint32_t*)epc >> 6) & 0x3FF;
		if (code == 7)
			return "Integer divide by zero";
	}	return exceptionMap[ex->code];
	default:
		return exceptionMap[ex->code];
	}
}

/**
 * @brief Fetch relevant registers
 *
 * @param[out] e
 *             Pointer to structure describing the exception
 * @param[in]  type
 *             Exception type.  Either #EXCEPTION_TYPE_CRITICAL or 
 *             #EXCEPTION_TYPE_RESET
 * @param[in]  regs
 *             CPU register status at exception time
 */
static void __fetch_regs(exception_t* e, int32_t type, reg_block_t *regs)
{
	e->regs = regs;
	e->type = type;
	e->code = C0_GET_CAUSE_EXC_CODE(e->regs->cr);
	e->info = __get_exception_name(e);
}

/**
 * @brief Respond to a critical exception
 */
void __onCriticalException(reg_block_t* regs)
{
	exception_t e;

	if(!__exception_handler) { return; }

	__fetch_regs(&e, EXCEPTION_TYPE_CRITICAL, regs);
	__exception_handler(&e);
}

void register_syscall_handler( syscall_handler_t handler, uint32_t first_code, uint32_t last_code )
{
	assertf(first_code <= 0xFFFFF, "The maximum allowed syscall code is 0xFFFFF (requested: %05lx)\n", first_code);
	assertf(last_code <= 0xFFFFF, "The maximum allowed syscall code is 0xFFFFF (requested: %05lx)\n", first_code);
	assertf(first_code <= last_code, "Invalid range for syscall handler (first: %05lx, last: %05lx)\n", first_code, last_code);

	for (int i=0;i<MAX_SYSCALL_HANDLERS;i++)
	{
		if (!__syscall_handlers[i].handler)
		{
			__syscall_handlers[i].first_code = first_code;
			__syscall_handlers[i].last_code = last_code;
			__syscall_handlers[i].handler = handler;
			return;
		}
		else if (__syscall_handlers[i].first_code <= last_code && __syscall_handlers[i].last_code >= first_code)
		{
			assertf(0, "Syscall handler %p already registered for code range %05lx - %05lx", 
				__syscall_handlers[i].handler, __syscall_handlers[i].first_code, __syscall_handlers[i].last_code);
		}
	}
	assertf(0, "Too many syscall handlers\n");
}


/**
 * @brief Respond to a syscall exception.
 * 
 * Calls the handlers registered by #register_syscall_handler.
 * 
 * As a special case, if the specified syscall code is 1, the kernel scheduling
 * function will be called. That function potentially returns a new stack pointer
 * to be used if switching threads.
 */
reg_block_t* __onSyscallException( reg_block_t* regs )
{
	exception_t e;

	__fetch_regs(&e, EXCEPTION_TYPE_SYSCALL, regs);

	// Fetch the syscall code from the opcode
	uint32_t epc = e.regs->epc;
	uint32_t opcode = *(uint32_t*)epc;
	uint32_t code = (opcode >> 6) & 0xfffff;

	if (code == 0)
		return __kthread_syscall_schedule(regs);

	bool called = false;
	for (int i=0; i<MAX_SYSCALL_HANDLERS; i++)
	{
		if (__syscall_handlers[i].handler && 
		    __syscall_handlers[i].first_code <= code &&
			__syscall_handlers[i].last_code >= code)
		{
			__syscall_handlers[i].handler(&e, code);
			called = true;
		}
	}

	if (!called)  {
		__onCriticalException(regs);
		return regs;
	}

	// Skip syscall opcode to continue execution
	e.regs->epc += 4;
	return regs;
}
