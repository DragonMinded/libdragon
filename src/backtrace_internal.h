#ifndef __LIBDRAGON_BACKTRACE_INTERNAL_H
#define __LIBDRAGON_BACKTRACE_INTERNAL_H

/** @brief The "type" of funciton as categorized by the backtrace heuristic (__bt_analyze_func) */
typedef enum {
    BT_FUNCTION,                ///< Regular function with a stack frame
    BT_FUNCTION_FRAMEPOINTER,   ///< The function uses the register fp as frame pointer (normally, this happens only when the function uses alloca)
    BT_EXCEPTION,               ///< This is an exception handler (inthandler.S)
    BT_LEAF                     ///< Leaf function (no calls), no stack frame allocated, sp/ra not modified
} bt_func_type;

/** @brief Description of a function for the purpose of backtracing (filled by __bt_analyze_func) */
typedef struct {
    bt_func_type type;       ///< Type of the function
    int stack_size;          ///< Size of the stack frame
    int ra_offset;           ///< Offset of the return address from the top of the stack frame
    int fp_offset;           ///< Offset of the saved fp from the top of the stack frame; this is != 0 only if the function modifies fp (maybe as a frame pointer, but not necessarily)
} bt_func_t;

bool __bt_analyze_func(bt_func_t *func, uint32_t *ptr, uint32_t func_start, bool from_exception);

/** @brief Like #backtrace, but start from an arbitrary context. Useful for backtracing a thread */
int __backtrace_from(void **buffer, int size, uint32_t *pc, uint32_t *sp, uint32_t *fp, uint32_t *exception_ra);

/**
 * @brief Return the symbol associated to a given address.
 * 
 * This function inspect the symbol table (if any) to search for the
 * specified address. It returns the function name the address belongs
 * to, and the offset within the function as a string in the format
 * "function_name+0x1234".
 * 
 * If the symbol table is not found in the rompack or the address is not found,
 * the return string is "???".
 * 
 * @param vaddr         Address to symbolize 
 * @param buf           Buffer where to store the result
 * @param size          Size of the buffer
 * @return char*        Pointer to the return string. This is within the provided
 *                      buffer, but not necessarily at the beginning because of DMA
 *                      alignment constraints.
 */
char* __symbolize(void *vaddr, char *buf, int size);

#endif
