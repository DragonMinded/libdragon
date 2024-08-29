/**
 * @file backtrace.c
 * @brief Backtrace (call stack) support
 * @ingroup backtrace
 * 
 * This file contains the implementation of the backtrace support. See
 * backtrace.h for an overview of the API. Here follows some implementation
 * details.
 * 
 * Backtrace 
 * =========
 * MIPS ABIs do not generally provide a way to walk the stack, as the frame
 * pointer is not guaranteed to be present. It is possible to force its presence
 * via "-fno-omit-frame-pointer", but we tried to provide a solution that works
 * with standard compilation settings.
 * 
 * To perform backtracing, we scan the code backward starting from the return address
 * of each frame. While scanning, we note some special instructions that we look
 * for. The two main instructions that we look for are `sd ra, offset(sp)` which is
 * used to save the previous return address to the stack, and `addiu sp, sp, offset`
 * which creates the stack frame for the current function. When we find both, we know
 * how to get back to the previous frame. 
 * 
 * Notice that this also works through exceptions, as the exception handler does create
 * a stack frame exactly like a standard function (see inthandler.S). 
 * 
 * Only a few functions do use a frame pointer: those that allocate a runtime-calculated
 * amount of stack (eg: using alloca). Because of this, we actually look for usages
 * of the frame pointer register fp, and track those as well to be able to correctly
 * walk the stack in those cases.
 * 
 * Symbolization
 * =============
 * To symbolize the backtrace, we use a symbol table file (SYMT) that is generated
 * by the n64sym tool during the build process. The symbol table is put into the
 * rompak (see rompak_internal.h) and is structured in a way that can be queried
 * directly from ROM, without even allocating memory. This is especially useful
 * to provide backtrace in catastrophic situations where the heap is not available.
 * 
 * The symbol table file contains the source code references (function name, file name,
 * line number) for a number of addresses in the ROM. Since it would be impractical to
 * save information for all the addresses in the text segment, only special addresses
 * are saved: in particular, those where a function call is made (ie: the address of
 * JAL / JALR instructions), which are the ones that are commonly found in backtraces
 * and thus need to be symbolized. In addition to these, the symbol table contains
 * also information associated to the addresses that mark the start of each function,
 * so that it's always possible to infer the function a certain address belongs to.
 * 
 * Given that not all addresses are saved, it is important to provide accurate
 * source code references for stack frames that are interrupted by interrupts or 
 * exceptions; in those cases, the symbolization will simply return the function name
 * the addresses belongs to, without any source code reference.
 * 
 * To see more details on how the symbol table is structured in the ROM, see
 * #symtable_header_t and the source code of the n64sym tool.
 * 
 */
#include <stdint.h>
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>
#include "backtrace.h"
#include "backtrace_internal.h"
#include "debug.h"
#include "n64sys.h"
#include "dma.h"
#include "utils.h"
#include "exception.h"
#include "interrupt.h"
#include "rompak_internal.h"
#include "dlfcn_internal.h"

/** @brief Enable to debug why a backtrace is wrong */
#define BACKTRACE_DEBUG 0

/** @brief Function alignment enfored by the compiler (-falign-functions). 
 * 
 * @note This must be kept in sync with n64.mk.
 */
#define FUNCTION_ALIGNMENT      32

/** 
 * @brief Symbol table file header
 * 
 * The SYMT file is made of three main tables:
 * 
 * * Address table: this is a sequence of 32-bit integers, each representing an address in the ROM.
 *   The table is sorted in ascending order to allow for binary search. Moreover, the lowest 2 bits
 *   of each address can store additional information: If bit 0 is set to 1, the address is the start
 *   of a function. If bit 1 is set to 1, the address is an inline duplicate. In fact, there might be
 *   multiple symbols at the same address for inlined functions, so we need one entry in this table
 *   for each entry; all of them will have the same address, and all but the last one will have bit
 *   1 set to 1.
 * * Symbol table: this is a sequence of symbol table entries, each representing a symbol. The size
 *   of this table (in number of entries) is exactly the same as the address table. In fact, each
 *   address of the address table can be thought of as an external member of this structure; it's
 *   split externally to allow for efficiency reasons. Each entry stores the function name, 
 *   the source file name and line number, and the binary offset of the symbol within the containing
 *   function.
 * * String table: this table can be thought as a large buffer holding all the strings needed by all
 *   symbol entries (function names and file names). Each symbol entry stores a string as an offset
 *   within the symbol table and a length. This allows to reuse the same string (or prefix thereof)
 *   multiple times. Notice that strings are not null terminated in the string table.
 * 
 * The SYMT file is generated by the n64sym tool during the build process.
 */
typedef struct {
    char head[4];           ///< Magic ID "SYMT"
    uint32_t version;       ///< Version of the symbol table
    uint32_t addrtab_off;   ///< Offset of the address table in the file
    uint32_t addrtab_size;  ///< Size of the address table in the file (number of entries)
    uint32_t symtab_off;    ///< Offset of the symbol table in the file
    uint32_t symtab_size;   ///< Size of the symbol table in the file (number of entries); always equal to addrtab_size.
    uint32_t strtab_off;    ///< Offset of the string table in the file
    uint32_t strtab_size;   ///< Size of the string table in the file (number of entries)
} symtable_header_t;

/** @brief Symbol table entry **/
typedef struct {
    uint32_t func_sidx;     ///< Offset of the function name in the string table
    uint32_t file_sidx;     ///< Offset of the file name in the string table
    uint16_t func_len;      ///< Length of the function name
    uint16_t file_len;      ///< Length of the file name
    uint16_t line;          ///< Line number (or 0 if this symbol generically refers to a whole function)
    uint16_t func_off;      ///< Offset of the symbol within its function
} symtable_entry_t;

/** 
 * @brief Entry in the address table.
 * 
 * This is an address in RAM, with the lowest 2 bits used to store additional information.
 * See the ADDRENTRY_* macros to access the various components.
 */
typedef uint32_t addrtable_entry_t;

#define ADDRENTRY_ADDR(e)       ((e) & ~3)     ///< Address (without the flags9)
#define ADDRENTRY_IS_FUNC(e)    ((e) &  1)     ///< True if the address is the start of a function
#define ADDRENTRY_IS_INLINE(e)  ((e) &  2)     ///< True if the address is an inline duplicate

#define MIPS_OP_ADDIU_SP(op)   (((op) & 0xFFFF0000) == 0x27BD0000)   ///< Matches: addiu $sp, $sp, imm
#define MIPS_OP_DADDIU_SP(op)  (((op) & 0xFFFF0000) == 0x67BD0000)   ///< Matches: daddiu $sp, $sp, imm
#define MIPS_OP_JR_RA(op)      (((op) & 0xFFFFFFFF) == 0x03E00008)   ///< Matches: jr $ra
#define MIPS_OP_SD_RA_SP(op)   (((op) & 0xFFFF0000) == 0xFFBF0000)   ///< Matches: sd $ra, imm($sp)
#define MIPS_OP_SD_FP_SP(op)   (((op) & 0xFFFF0000) == 0xFFBE0000)   ///< Matches: sd $fp, imm($sp)
#define MIPS_OP_LUI_GP(op)     (((op) & 0xFFFF0000) == 0x3C1C0000)   ///< Matches: lui $gp, imm
#define MIPS_OP_NOP(op)        ((op) == 0x00000000)                  ///< Matches: nop
#define MIPS_OP_MOVE_FP_SP(op) ((op) == 0x03A0F025)                  ///< Matches: move $fp, $sp

/** @brief Exception handler (see inthandler.S) */
extern uint32_t inthandler[];
/** @brief End of exception handler (see inthandler.S) */
extern uint32_t inthandler_end[];

/** @brief Start of main executable text section */
extern uint32_t __text_start[];
/** @brief End of main executable text section */
extern uint32_t __text_end[];

/** @brief Address of the SYMT symbol table in the rompak. */
static uint32_t SYMT_ROM = 0xFFFFFFFF;

/** @brief Placeholder used in frames where symbols are not available */
static const char *UNKNOWN_SYMBOL = "???";

/** @brief Base address for addresses in address table */
static uint32_t addrtable_base = 0;


/** @brief Check if addr is a valid PC address */
static bool is_valid_address(uint32_t addr)
{
    // TODO: for now we only handle RAM (cached access). This should be extended to handle
    // TLB-mapped addresses for instance.
    return addr >= 0x80000400 && addr < 0x80800000 && (addr & 3) == 0;
}

/** @brief Check if addr is inside main executable text section */
static bool is_main_exe_text_address(uint32_t addr)
{
    // TODO: for now we only handle RAM (cached access). This should be extended to handle
    // TLB-mapped addresses for instance.
    return addr >= (uint32_t)__text_start && addr < (uint32_t)__text_end;
}

/** 
 * @brief Open the SYMT symbol table in the rompak.
 * 
 * If not found, return a null header.
 */
static symtable_header_t symt_open(void *addr) {
	if(is_main_exe_text_address((uint32_t)addr)) {
		//Open SYMT from rompak
        static uint32_t mainexe_symt = 0xFFFFFFFF;
        if (mainexe_symt == 0xFFFFFFFF) {
            mainexe_symt = rompak_search_ext(".sym");
            if (!mainexe_symt)
                debugf("backtrace: no symbol table found in the rompak\n");
        }
        addrtable_base = 0;
        SYMT_ROM = mainexe_symt;
	} else {
		dl_module_t *module = NULL;
		if(__dl_lookup_module) {
			module = __dl_lookup_module(addr);
		}
		if(module && module->sym_romofs != 0) {
			//Read module SYMT
			SYMT_ROM = module->sym_romofs;
			addrtable_base = (uint32_t)module->prog_base;
		} else {
			SYMT_ROM = 0;
			addrtable_base = 0;
		}
	}
    
    if (!SYMT_ROM) {
        return (symtable_header_t){0};
    }

    symtable_header_t alignas(8) symt_header;
    data_cache_hit_writeback_invalidate(&symt_header, sizeof(symt_header));
    dma_read(&symt_header, SYMT_ROM, sizeof(symtable_header_t));

    if (symt_header.head[0] != 'S' || symt_header.head[1] != 'Y' || symt_header.head[2] != 'M' || symt_header.head[3] != 'T') {
        debugf("backtrace: invalid symbol table found at 0x%08lx\n", SYMT_ROM);
        SYMT_ROM = 0;
        return (symtable_header_t){0};
    }
    if (symt_header.version != 2) {
        debugf("backtrace: unsupported symbol table version %ld -- please update your n64sym tool\n", symt_header.version);
        SYMT_ROM = 0;
        return (symtable_header_t){0};
    }

    return symt_header;
}

/**
 * @brief Return an entry in the address table by index
 * 
 * @param symt      SYMT file header
 * @param idx       Index of the entry to return
 * @return addrtable_entry_t  Entry of the address table
 */
static addrtable_entry_t symt_addrtab_entry(symtable_header_t *symt, int idx)
{
    assert(idx >= 0 && idx < symt->addrtab_size);
    return addrtable_base+io_read(SYMT_ROM + symt->addrtab_off + idx * 4);
}

/**
 * @brief Search the SYMT address table for the given address.
 * 
 * Run a binary search to find the entry in the table. If there is a single exact match,
 * the entry is returned. If there are multiple entries with the same address, the first
 * entry is returned (this is the case for inlined functions: so some entries following
 * the current one will have the same address). If there is no exact match, the entry
 * with the biggest address just before the given address is returned.
 *
 * @param symt      SYMT file header
 * @param addr      Address to search for
 * @param idx       If not null, will be set to the index of the entry found (or the index just before)
 * @return          The found entry (or the entry just before)
 */
static addrtable_entry_t symt_addrtab_search(symtable_header_t *symt, uint32_t addr, int *idx)
{
    int min = 0;
    int max = symt->addrtab_size - 1;
    while (min < max) {
        int mid = (min + max) / 2;
        addrtable_entry_t entry = symt_addrtab_entry(symt, mid);
        if (addr <= ADDRENTRY_ADDR(entry))
            max = mid;
        else
            min = mid + 1;
    }
    addrtable_entry_t entry = symt_addrtab_entry(symt, min);
    if (min > 0 && ADDRENTRY_ADDR(entry) > addr)
        entry = symt_addrtab_entry(symt, --min);
    if (idx) *idx = min;
    return entry;
}


/**
 * @brief Fetch a string from the string table
 * 
 * @param symt  SYMT file
 * @param sidx  Index of the first character of the string in the string table
 * @param slen  Length of the string
 * @param buf   Destination buffer
 * @param size  Size of the destination buffer
 * @return char*  Fetched string within the destination buffer (might not be at offset 0 for alignment reasons)
 */
static char* symt_string(symtable_header_t *symt, int sidx, int slen, char *buf, int size)
{
    // Align 2-byte phase of the RAM buffer with the ROM address. This is required
    // for dma_read.
    int tweak = (sidx ^ (uint32_t)buf) & 1;
    char *func = buf + tweak; size -= tweak;
    int n = MIN(slen, size);

    data_cache_hit_writeback_invalidate(buf, size);
    dma_read(func, SYMT_ROM + symt->strtab_off + sidx, n);
    func[n] = 0;
    return func;
}

/**
 * @brief Fetch a symbol table entry from the SYMT file.
 * 
 * @param symt    SYMT file
 * @param entry   Output entry pointer
 * @param idx     Index of the entry to fetch
 */
static void symt_entry_fetch(symtable_header_t *symt, symtable_entry_t *entry, int idx)
{
    data_cache_hit_writeback_invalidate(entry, sizeof(symtable_entry_t));
    dma_read(entry, SYMT_ROM + symt->symtab_off + idx * sizeof(symtable_entry_t), sizeof(symtable_entry_t));
}

// Fetch the function name of an entry
static char* symt_entry_func(symtable_header_t *symt, symtable_entry_t *entry, uint32_t addr, char *buf, int size)
{
    if (addr >= (uint32_t)inthandler && addr < (uint32_t)inthandler_end) {
        // Special case exception handlers. This is just to show something slightly
        // more readable instead of "notcart+0x0" or similar assembly symbols
        snprintf(buf, size, "<EXCEPTION HANDLER>");
        return buf;
    } else {
        return symt_string(symt, entry->func_sidx, entry->func_len, buf, size);
    }
}

// Fetch the file name of an entry
static char* symt_entry_file(symtable_header_t *symt, symtable_entry_t *entry, uint32_t addr, char *buf, int size)
{
    return symt_string(symt, entry->file_sidx, entry->file_len, buf, size);
}

char* __symbolize(void *vaddr, char *buf, int size)
{
    symtable_header_t symt = symt_open(vaddr);
    if (symt.head[0]) {
        uint32_t addr = (uint32_t)vaddr;
        int idx = 0;
        addrtable_entry_t a = symt_addrtab_search(&symt, addr, &idx);
        while (!ADDRENTRY_IS_FUNC(a))
            a = symt_addrtab_entry(&symt, --idx);

        // Read the symbol name
        symtable_entry_t alignas(8) entry;
        symt_entry_fetch(&symt, &entry, idx);
        char *func = symt_entry_func(&symt, &entry, addr, buf, size-12);
        char lbuf[12];
        snprintf(lbuf, sizeof(lbuf), "+0x%lx", addr - ADDRENTRY_ADDR(a));
        return strcat(func, lbuf);
    }
    snprintf(buf, size, "%s", UNKNOWN_SYMBOL);
    return buf;
}

/**
 * @brief Analyze a function to find out its stack frame layout and properties (useful for backtracing).
 * 
 * This function implements the core heuristic used by the backtrace engine. It analyzes the actual
 * code of a function in memory instruction by instruction, trying to find out whether the function
 * uses a stack frame or not, whether it uses a frame pointer, and where the return address is stored.
 * 
 * Since we do not have DWARF informations or similar metadata, we can just do educated guesses. A
 * mistake in the heuristic will result probably in a wrong backtrace from this point on.
 * 
 * The heuristic works as follows:
 * 
 *  * Most functions do have a stack frame. In fact, 99.99% of the functions you can find in a call stack
 *    must have a stack frame, because the only functions without a stack frame are leaf functions (functions
 *    that do not call other functions), which in turns can never be part of a stack trace.
 *  * The heuristic walks the function code backwards, looking for the stack frame. Specifically, it looks
 *    for an instruction saving the RA register to the stack (eg: `sd $ra, nn($sp)`), and an instruction
 *    creating the stack frame (eg: `addiu $sp, $sp, -nn`). Once both are found, the heuristic knows how to
 *    fill in `.stack_size` and `.ra_offset` fields of the function description structure, and it can stop.
 *  * Some functions also modify $fp (the frame pointer register): sometimes, they just use it as one additional
 *    free register, and other times they really use it as frame pointer. If the heuristic finds the
 *    instruction `move $fp, $sp`, it knows that the function uses $fp as frame pointer, and will mark
 *    the function as BT_FUNCTION_FRAMEPOINTER. In any case, the field `.fp_offset` will be filled in
 *    with the offset in the stack where $fp is stored, so that the backtrace engine can track the
 *    current value of the register in any case.
 *  * The 0.01% of the functions that do not have a stack frame but appear in the call stack are leaf
 *    functions interrupted by exceptions. Leaf functions pose two important problems: first, $ra is
 *    not saved into the stack so there is no way to know where to go back. Second, there is no clear
 *    indication where the function begins (as we normally stops analysis when we see the stack frame
 *    creation). So in this case the heuristic would fail. We rely thus on two hints coming from the caller:
 *    * First, we expect the caller to set from_exception=true, so that we know that we might potentially
 *      deal with a leaf function. 
 *    * Second, the caller should provide the function start address, so that we stop the analysis when
 *      we reach it, and mark the function as BT_LEAF.
 *    * If the function start address is not provided (because e.g. the symbol table was not found and
 *      thus we have no information about function starts), the last ditch heuristic is to look for
 *      the nops that are normally used to align the function start to the FUNCTION_ALIGNMENT boundary.
 *      Obviously this is a very fragile heuristic (it will fail if the function required no nops to be
 *      properly aligned), but it is the best we can do. Worst case, in this specific case of a leaf
 *      function interrupted by the exception, the stack trace will be wrong from this point on.
 * 
 * @param func                        Output function description structure
 * @param ptr                         Pointer to the function code at the point where the backtrace starts.
 *                                    This is normally the point where a JAL opcode is found, as we are walking
 *                                    up the call stack.
 * @param func_start                  Start of the function being analyzed. This is optional: the heuristic can work
 *                                    without this hint, but it is useful in certain situations (eg: to better
 *                                    walk up after an exception).
 * @param from_exception              If true, this function was interrupted by an exception. This is a hint that
 *                                    the function *might* even be a leaf function without a stack frame, and that
 *                                    we must use special heuristics for it.
 * 
 * @return true if the backtrace can continue, false if must be aborted (eg: we are within invalid memory)
 */
bool __bt_analyze_func(bt_func_t *func, uint32_t *ptr, uint32_t func_start, bool from_exception)
{
    *func = (bt_func_t){
        .type = (ptr >= inthandler && ptr < inthandler_end) ? BT_EXCEPTION : BT_FUNCTION,
        .stack_size = 0, .ra_offset = 0, .fp_offset = 0
    };

    uint32_t addr = (uint32_t)ptr;
    while (1) {
        // Validate that we can dereference the virtual address without raising an exception
        // TODO: enhance this check with more valid ranges.
        if (!is_valid_address(addr)) {
            // This address is invalid, probably something is corrupted. Avoid looking further.
            debugf("backtrace: interrupted because of invalid return address 0x%08lx\n", addr);
            return false;
        }
        uint32_t op = *(uint32_t*)addr;
        if (MIPS_OP_ADDIU_SP(op) || MIPS_OP_DADDIU_SP(op)) {
            // Extract the stack size only from the start of the function, where the
            // stack is allocated (negative value). This is important because the RA
            // could point to a leaf basis block at the end of the function (like in the
            // assert case), and if we picked the positive ADDIU SP at the end of the
            // proper function body, we might miss a fp_offset.
            if (op & 0x8000)
                func->stack_size = -(int16_t)(op & 0xFFFF);
        } else if (MIPS_OP_SD_RA_SP(op)) {
            func->ra_offset = (int16_t)(op & 0xFFFF) + 4; // +4 = load low 32 bit of RA
            // If we found a stack size, it might be a red herring (an alloca); we need one
            // happening "just before" sd ra,xx(sp)
            func->stack_size = 0;
        } else if (MIPS_OP_SD_FP_SP(op)) {
            func->fp_offset = (int16_t)(op & 0xFFFF) + 4; // +4 = load low 32 bit of FP
        } else if (MIPS_OP_LUI_GP(op)) {
            // Loading gp is commonly done in _start, so it's useless to go back more
            return false;
        } else if (MIPS_OP_MOVE_FP_SP(op)) {
            // This function uses the frame pointer. Uses that as base of the stack.
            // Even with -fomit-frame-pointer (default on our toolchain), the compiler
            // still emits a framepointer for functions using a variable stack size
            // (eg: using alloca() or VLAs).
            func->type = BT_FUNCTION_FRAMEPOINTER;
        } 
        // We found the stack frame size and the offset of the return address in the stack frame
        // We can stop looking and process the frame
        if (func->stack_size != 0 && func->ra_offset != 0)
            break;
        if (from_exception) {
            // The function we are analyzing was interrupted by an exception, so it might
            // potentially be a leaf function (no stack frame). We need to make sure to stop
            // at the beginning of the function and mark it as leaf function. Use
            // func_start if specified, or try to guess using the nops used to align the function
            // (crossing fingers that they're there).
            if (addr == func_start) {
                // The frame that was interrupted by an interrupt handler is a special case: the
                // function could be a leaf function with no stack. If we were able to identify
                // the function start (via the symbol table) and we reach it, it means that
                // we are in a real leaf function.
                func->type = BT_LEAF;
                break;
            } else if (!func_start && MIPS_OP_NOP(op) && (addr + 4) % FUNCTION_ALIGNMENT == 0) {
                // If we are in the frame interrupted by an interrupt handler, and we does not know
                // the start of the function (eg: no symbol table), then try to stop by looking for
                // a NOP that pads between functions. Obviously the NOP we find can be either a false
                // positive or a false negative, but we can't do any better without symbols.
                func->type = BT_LEAF;
                break;
            }
        }
        addr -= 4;
    }
    return true;
}

static void backtrace_foreach(void (*cb)(void *arg, void *ptr), void *arg, uint32_t *ra, uint32_t *sp, uint32_t *fp, uint32_t *exception_ra)
{
    /*
     * This function is called in very risky contexts, for instance as part of an exception
     * handler or during an assertion. We try to always provide as much information as
     * possible in these cases, with graceful degradation if something more elaborate cannot
     * be extracted. Thus, this function:
     * 
     *  * Must not use malloc(). The heap might be corrupted or empty.
     *  * Must not use assert(), because that might trigger recursive assertions.
     *  * Must avoid raising exceptions. Specifically, it must avoid risky memory accesses
     *    to wrong addresses.
     */

    #if BACKTRACE_DEBUG
    debugf("backtrace: start\n"); 
    #endif

    uint32_t func_start = 0;            // Start of the current function (when known)

    // Start calling the callback for the backtrace entry point
    cb(arg, ra);

    while (1) {
        // Analyze the function pointed by ra, passing information about the previous exception frame if any.
        // If the analysis fail (for invalid memory accesses), stop right away.
        bt_func_t func; 
        if (!__bt_analyze_func(&func, ra, func_start, exception_ra))
            return;

        #if BACKTRACE_DEBUG
        debugf("backtrace: %s, ra=%p, sp=%p, fp=%p ra_offset=%d, fp_offset=%d, stack_size=%d\n", 
            func.type == BT_FUNCTION ? "BT_FUNCTION" : (func.type == BT_EXCEPTION ? "BT_EXCEPTION" : (func.type == BT_FUNCTION_FRAMEPOINTER ? "BT_FRAMEPOINTER" : "BT_LEAF")),
            ra, sp, fp, func.ra_offset, func.fp_offset, func.stack_size);
        #endif

        switch (func.type) {
            case BT_FUNCTION_FRAMEPOINTER:
                if (!func.fp_offset) {
                    debugf("backtrace: framepointer used but not saved onto stack at %p\n", ra);
                } else {
                    // Use the frame pointer to refer to the current frame.
                    sp = fp;
                    if (!is_valid_address((uint32_t)sp)) {
                        debugf("backtrace: interrupted because of invalid frame pointer 0x%08lx\n", (uint32_t)sp);
                        return;
                    }
                }
                // FALLTHROUGH!
            case BT_FUNCTION:
                if (func.fp_offset)
                    fp = *(uint32_t**)((uint32_t)sp + func.fp_offset);
                ra = *(uint32_t**)((uint32_t)sp + func.ra_offset) - 2;
                sp = (uint32_t*)((uint32_t)sp + func.stack_size);
                exception_ra = NULL;
                func_start = 0;
                break;
            case BT_EXCEPTION: {
                // Exception frame. We must return back to EPC, but let's keep the
                // RA value. If the interrupted function is a leaf function, we
                // will need it to further walk back.
                // Notice that FP is a callee-saved register so we don't need to
                // recover it from the exception frame (also, it isn't saved there
                // during interrupts).
                exception_ra = *(uint32_t**)((uint32_t)sp + func.ra_offset);

                // Read EPC from exception frame and adjust it with CAUSE BD bit
                ra = *(uint32_t**)((uint32_t)sp + offsetof(reg_block_t, epc) + 32);
                uint32_t cause = *(uint32_t*)((uint32_t)sp + offsetof(reg_block_t, cr) + 32);
                if (cause & C0_CAUSE_BD) ra++;

                sp = (uint32_t*)((uint32_t)sp + func.stack_size);

                // Special case: if the exception is due to an invalid EPC
                // (eg: a null function pointer call), we can rely on RA to get
                // back to the caller. This assumes that we got there via a function call
                // rather than a raw jump, but that's a reasonable assumption. It's anyway
                // the best we can do.
                if ((C0_GET_CAUSE_EXC_CODE(cause) == EXCEPTION_CODE_TLB_LOAD_I_MISS ||
                    C0_GET_CAUSE_EXC_CODE(cause) == EXCEPTION_CODE_LOAD_I_ADDRESS_ERROR) && 
                    !is_valid_address((uint32_t)ra)) {
                    
                    // Store the invalid address in the backtrace, so that it will appear in dumps.
                    // This makes it easier for the user to understand the reason for the exception.
                    cb(arg, ra);
                    #if BACKTRACE_DEBUG
                    debugf("backtrace: %s, ra=%p, sp=%p, fp=%p ra_offset=%d, fp_offset=%d, stack_size=%d\n", 
                        "BT_INVALID", ra, sp, fp, func.ra_offset, func.fp_offset, func.stack_size);
                    #endif
                    
                    ra = exception_ra - 2;

                    // The function that jumped into an invalid PC was not interrupted by the exception: it
                    // is a regular function
                    // call now.
                    exception_ra = NULL;
                    break;
                }

                // The next frame might be a leaf function, for which we will not be able
                // to find a stack frame. It is useful to try finding the function start.
                // Try to open the symbol table: if we find it, we can search for the start
                // address of the function.
                symtable_header_t symt = symt_open(ra);
                if (symt.head[0]) {
                    int idx;
                    addrtable_entry_t entry = symt_addrtab_search(&symt, (uint32_t)ra, &idx);
                    while (!ADDRENTRY_IS_FUNC(entry))
                        entry = symt_addrtab_entry(&symt, --idx);
                    func_start = ADDRENTRY_ADDR(entry);
                    #if BACKTRACE_DEBUG
                    debugf("Found interrupted function start address: %08lx\n", func_start);
                    #endif
                }
            }   break;
            case BT_LEAF:
                ra = exception_ra - 2;
                // A leaf function has no stack. On the other hand, an exception happening at the
                // beginning of a standard function (before RA is saved), does have a stack but
                // will be marked as a leaf function. In this case, we mus update the stack pointer.
                sp = (uint32_t*)((uint32_t)sp + func.stack_size);
                exception_ra = NULL;
                func_start = 0;
                break;
        }

        // Call the callback with this stack frame
        cb(arg, ra);
    }
}

int backtrace(void **buffer, int size)
{
    int i = -2; // skip backtrace_foreach() and backtrace())
    void cb(void *arg, void *ptr) {
        if (i >= size) return;
        if (i >= 0)
            buffer[i] = ptr;
        i++;
    }

    // Current value of SP/RA/FP registers.
    uint32_t *sp, *fp;
    asm volatile (
        "move %0, $sp\n"
        "move %1, $fp\n"
        : "=r"(sp), "=r"(fp)
    );

    // Start from the backtrace function itself. Put the start pointer
    // somewhere after the initial prolog, so that we parse the prolog
    // itself to find sp/fp/ra offsets.
    // Since we don't come from an exception, exception_ra must be NULL.
    uint32_t *pc = (uint32_t*)backtrace + 24;

    backtrace_foreach(cb, NULL, pc, sp, fp, NULL);
    return i;
}

int __backtrace_from(void **buffer, int size, uint32_t *pc, uint32_t *sp, uint32_t *fp, uint32_t *exception_ra)
{
    int i = 0;
    void cb(void *arg, void *ptr) {
        if (i >= size) return;
        if (i >= 0)
            buffer[i] = ptr;
        i++;
    }

    backtrace_foreach(cb, NULL, pc, sp, fp, exception_ra);
    return i;
}


static void format_entry(void (*cb)(void *, backtrace_frame_t *), void *cb_arg, 
    symtable_header_t *symt, int idx, uint32_t addr, uint32_t offset, bool is_func, bool is_inline)
{       
    symtable_entry_t alignas(8) entry;
    symt_entry_fetch(symt, &entry, idx);

    char alignas(8) file_buf[entry.file_len + 2];
    char alignas(8) func_buf[MAX(entry.func_len + 2, 32)];

    cb(cb_arg, &(backtrace_frame_t){
        .addr = addr,
        .func_offset = offset ? offset : entry.func_off,
        .func = symt_entry_func(symt, &entry, addr, func_buf, sizeof(func_buf)),
        .source_file = symt_entry_file(symt, &entry, addr, file_buf, sizeof(file_buf)),
        .source_line = is_func ? 0 : entry.line,
        .is_inline = is_inline,
    });
}

bool backtrace_symbols_cb(void **buffer, int size, uint32_t flags,
    void (*cb)(void *, backtrace_frame_t *), void *cb_arg)
{
    for (int i=0; i<size; i++) {
        uint32_t needle = (uint32_t)buffer[i];
        // Open the symbol table. If not found, we will still invoke the
        // callback but using unsymbolized addresses.
        symtable_header_t symt_header = symt_open(buffer[i]);
        bool has_symt = symt_header.head[0];
        if (!is_valid_address(needle)) {
            // If the address is before the first symbol, we call it a NULL pointer, as that is the most likely case
            cb(cb_arg, &(backtrace_frame_t){
                .addr = needle,
                .func_offset = needle,
                .func = needle < 128 ? "<NULL POINTER>" : "<INVALID ADDRESS>",
                .source_file = UNKNOWN_SYMBOL, .source_line = 0, .is_inline = false
            });
            continue;
        }
        if (!has_symt) {
            // No symbol table. Call the callback with a dummy entry which just contains the address
            bool exc = (needle >= (uint32_t)inthandler && needle < (uint32_t)inthandler_end);
            cb(cb_arg, &(backtrace_frame_t){
                .addr = needle,
                .func = exc ? "<EXCEPTION HANDLER>" : UNKNOWN_SYMBOL, .func_offset = 0,
                .source_file = UNKNOWN_SYMBOL, .source_line = 0, .is_inline = false
            });
            continue;
        }
        int idx; addrtable_entry_t a;
        a = symt_addrtab_search(&symt_header, needle, &idx);

        if (ADDRENTRY_ADDR(a) == needle) {
            // Found an entry at this address. Go through all inlines for this address.
            while (1) {
                format_entry(cb, cb_arg, &symt_header, idx, needle, 0, false, ADDRENTRY_IS_INLINE(a));
                if (!ADDRENTRY_IS_INLINE(a)) break;
                a = symt_addrtab_entry(&symt_header, ++idx);
            }
        } else {
            // Search the containing function
            while (!ADDRENTRY_IS_FUNC(a))
                a = symt_addrtab_entry(&symt_header, --idx);
            format_entry(cb, cb_arg, &symt_header, idx, needle, needle - ADDRENTRY_ADDR(a), true, false);
        }
    }
    return true;
}

char** backtrace_symbols(void **buffer, int size)
{
    const int MAX_FILE_LEN = 120;
    const int MAX_FUNC_LEN = 120;
    const int MAX_SYM_LEN = MAX_FILE_LEN + MAX_FUNC_LEN + 24;
    char **syms = malloc(2 * size * (sizeof(char*) + MAX_SYM_LEN));
    char *out = (char*)syms + size*sizeof(char*);
    int level = 0;

    void cb(void *arg, backtrace_frame_t *frame) {
        int n = snprintf(out, MAX_SYM_LEN,
            "%s+0x%lx (%s:%d) [0x%08lx]", frame->func, frame->func_offset, frame->source_file, frame->source_line, frame->addr);
        if (frame->is_inline)
            out[-1] = '\n';
        else
            syms[level++] = out;
        out += n + 1;
    }

    backtrace_symbols_cb(buffer, size, 0, cb, NULL);
    return syms;
}

void backtrace_frame_print(backtrace_frame_t *frame, FILE *out)
{
    fprintf(out, "%s+0x%lx (%s:%d) [0x%08lx]%s", 
        frame->func, frame->func_offset, 
        frame->source_file, frame->source_line,
        frame->addr, frame->is_inline ? " (inline)" : "");
}

void backtrace_frame_print_compact(backtrace_frame_t *frame, FILE *out, int width)
{
    const char *source_file = frame->source_file;
    int len = strlen(frame->func) + strlen(source_file);
    bool ellipsed = false;
    if (len > width && source_file) {
        source_file += len - (width - 8);
        ellipsed = true;
    }
    if (frame->func != UNKNOWN_SYMBOL) fprintf(out, "%s ", frame->func);
    if (source_file != UNKNOWN_SYMBOL) fprintf(out, "(%s%s:%d)", ellipsed ? "..." : "", source_file, frame->source_line);
    if (frame->func == UNKNOWN_SYMBOL || source_file == UNKNOWN_SYMBOL)
        fprintf(out, "[0x%08lx]", frame->addr);
    fprintf(out, "\n");
}
