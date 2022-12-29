#ifndef __LIBDRAGON_BACKTRACE_INTERNAL_H
#define __LIBDRAGON_BACKTRACE_INTERNAL_H

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
