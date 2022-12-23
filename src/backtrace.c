#include <stdint.h>
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>
#include "backtrace.h"
#include "debug.h"
#include "n64sys.h"
#include "dma.h"
#include "utils.h"
#include "exception.h"
#include "interrupt.h"
#include "rompak_internal.h"

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
 * The SYMT file is made of three main table:
 * 
 * * Address table: this is a sequence of 32-bit integers, each representing an address in the ROM.
 *   The table is sorted in ascending order to allow for binary search. Morever, the lowest 2 bits
 *   of each address can store additional information: If bit 0 is set to 1, the address is the start
 *   of a function. If bit 1 is set to 1, the address is an inline duplicate. In fact, there might be
 *   multiple symbols at the same address for inlined functions, so we need one entry in this table
 *   for each entry; all of them will have the same address, and all but the first one will have bit
 *   1 set to 1.
 * * Symbol table: this is a sequence of symbol table entries, each representing a symbol. The size
 *   of this table (in number of entries) is exactly the same as the address table. In fact, each
 *   address of the address table can be thought of as an external member of this structure; it's
 *   split externally to allow for efficiency reasons. Each entry stores the function name, 
 *   the source file name and line number, and the binary offset of the symbol within the containing
 *   function.
 * * String table: This tables can be thought as a large buffer holding all the strings needed by all
 *   symbol entries (function names and file names). Each symbol entry stores a string as an index
 *   within the symbol table and a length. This allows to reuse the same string (or prefix thereof)
 *   multiple times. Notice that strings are not null terminated in the string table.
 */
typedef struct alignas(8) {
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
    uint16_t func_sidx;     ///< Offset of the function name in the string table
    uint16_t func_len;      ///< Length of the function name
    uint16_t file_sidx;     ///< Offset of the file name in the string table
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

#define MIPS_OP_ADDIU_SP(op)   (((op) & 0xFFFF0000) == 0x27BD0000)   // addiu $sp, $sp, imm
#define MIPS_OP_JR_RA(op)      (((op) & 0xFFFF0000) == 0x03E00008)   // jr $ra
#define MIPS_OP_SD_RA_SP(op)   (((op) & 0xFFFF0000) == 0xFFBF0000)   // sd $ra, imm($sp)
#define MIPS_OP_SD_FP_SP(op)   (((op) & 0xFFFF0000) == 0xFFBE0000)   // sd $fp, imm($sp)
#define MIPS_OP_LUI_GP(op)     (((op) & 0xFFFF0000) == 0x3C1C0000)   // lui $gp, imm
#define MIPS_OP_NOP(op)        ((op) == 0x00000000)                  // nop
#define MIPS_OP_MOVE_FP_SP(op) ((op) == 0x03A0F025)                  // move $fp, $sp

#define ABS(x)   ((x) < 0 ? -(x) : (x))

/** @brief Exception handler (see inthandler.S) */
extern uint32_t inthandler[];
/** @brief End of exception handler (see inthandler.S) */
extern uint32_t inthandler_end[];

/** @brief Address of the SYMT symbol table in the rompak. */
static uint32_t SYMT_ROM = 0xFFFFFFFF;

/** 
 * @brief Open the SYMT symbol table in the rompak.
 * 
 * If not found, return a null header.
 */
static symtable_header_t symt_open(void) {
    if (SYMT_ROM == 0xFFFFFFFF) {
        SYMT_ROM = rompak_search_ext(".sym");
        if (!SYMT_ROM)
            debugf("backtrace: no symbol table found in the rompak\n");
    }

    if (!SYMT_ROM) {
        return (symtable_header_t){0};
    }

    symtable_header_t symt_header;
    data_cache_hit_writeback_invalidate(&symt_header, sizeof(symt_header));
    dma_read_raw_async(&symt_header, SYMT_ROM, sizeof(symtable_header_t));
    dma_wait();

    if (symt_header.head[0] != 'S' || symt_header.head[1] != 'Y' || symt_header.head[2] != 'M' || symt_header.head[3] != 'T') {
        debugf("backtrace: invalid symbol table found at 0x%08lx\n", SYMT_ROM);
        SYMT_ROM = 0;
        return (symtable_header_t){0};
    }

    return symt_header;
}

static addrtable_entry_t symt_addrtab_entry(symtable_header_t *symt, int idx)
{
    return io_read(SYMT_ROM + symt->addrtab_off + idx * 4);
}

/**
 * @brief Search the SYMT address table for the given address (upper bound).
 * 
 * This uses a binary search to find the entry, using the "upper bound"
 * strategy. If the address is not found, the entry just after it is returned.
 * 
 * @param symt      SYMT file header
 * @param addr      Address to search for
 * @param idx       If not null, will be set to the index of the entry found (or the index just after)
 * @return addrtable_entry_t   The entry found, or 0 if the entry is not found
 */
static addrtable_entry_t symt_addrtab_search(symtable_header_t *symt, uint32_t addr, int *idx)
{
    int min = 0;
    int max = symt->addrtab_size - 1;
    while (min < max) {
        int mid = (min + max) / 2;
        addrtable_entry_t entry = symt_addrtab_entry(symt, mid);
        if (addr >= ADDRENTRY_ADDR(entry))
            min = mid + 1;
        else
            max = mid;
    }
    addrtable_entry_t entry;
    if (min < symt->addrtab_size) {
        entry = symt_addrtab_entry(symt, min);
        if (ADDRENTRY_ADDR(entry) <= addr)
            entry = symt_addrtab_entry(symt, ++min);            
    }
    if (idx) *idx = min;
    return (ADDRENTRY_ADDR(entry) == addr) ? entry : 0;
}


int backtrace(void **buffer, int size)
{
    uint32_t *sp, *ra, *fp;
    asm volatile (
        "move %0, $ra\n"
        "move %1, $sp\n"
        "move %2, $fp\n"
        : "=r"(ra), "=r"(sp), "=r"(fp)
    );

    int stack_size = 0;
    for (uint32_t *addr = (uint32_t*)backtrace; !stack_size; ++addr) {
        uint32_t op = *addr;
        if (MIPS_OP_ADDIU_SP(op))
            stack_size = ABS((int16_t)(op & 0xFFFF));
        else if (MIPS_OP_JR_RA(op))
            break;
    }

    uint32_t* interrupt_ra = NULL; uint32_t interrupt_rafunc_addr = 0;
    enum { BT_FUNCTION, BT_FUNCTION_FRAMEPOINTER, BT_EXCEPTION, BT_LEAF } bt_type;

    sp = (uint32_t*)((uint32_t)sp + stack_size);
    ra -= 2;
    for (int i=0; i<size; ++i) {
        buffer[i] = ra;
        bt_type = (ra >= inthandler && ra < inthandler_end) ? BT_EXCEPTION : BT_FUNCTION;

        uint32_t addr = (uint32_t)ra;
        int ra_offset = 0, fp_offset = 0, stack_size = 0;
        while (1) {
            if (addr < 0x80000400 || addr >= 0x80800000) {
                // This address is invalid, probably something is corrupted. Avoid looking further.
                debugf("backtrace: interrupted because of invalid return address 0x%08lx\n", addr);
                return i;
            }
            uint32_t op = *(uint32_t*)addr;
            if (MIPS_OP_ADDIU_SP(op)) {
                stack_size = ABS((int16_t)(op & 0xFFFF));
            } else if (MIPS_OP_SD_RA_SP(op)) {
                ra_offset = (int16_t)(op & 0xFFFF) + 4; // +4 = load low 32 bit of RA
            } else if (MIPS_OP_SD_FP_SP(op)) {
                fp_offset = (int16_t)(op & 0xFFFF) + 4; // +4 = load low 32 bit of FP
            } else if (MIPS_OP_LUI_GP(op)) {
                // Loading gp is commonly done in _start, so it's useless to go back more
                return i+1;
            } else if (MIPS_OP_MOVE_FP_SP(op)) {
                // This function uses the frame pointer. Uses that as base of the stack.
                // Even with -fomit-frame-pointer (default on our toolchain), the compiler
                // still emits a framepointer for functions using a variable stack size
                // (eg: using alloca() or VLAs).
                bt_type = BT_FUNCTION_FRAMEPOINTER;
			} else if (interrupt_ra && addr == interrupt_rafunc_addr) {
                // The frame that was interrupted by an interrupt handler is a special case: the
                // function could be a leaf function with no stack. If we were able to identify
                // the function start (via the symbol table) and we reach it, it means that
                // we are in a real leaf function.
                bt_type = BT_LEAF;
                break;
            } else if (interrupt_ra && !interrupt_rafunc_addr && MIPS_OP_NOP(op) && (addr + 4) % FUNCTION_ALIGNMENT == 0) {
                // If we are in the frame interrupted yb an interrupt handler, and we does not know
                // the start of the function (eg: no symbol table), then try to stop by looking for
                // a NOP that pads between functions. Obviously the NOP we find can be either a false
                // positive or a false negative, but we can't do any better without symbols.
                bt_type = BT_LEAF;
                break;
            }

            // We found the stack frame size and the offset of the return address in the stack frame
            // We can stop looking and process the frame
            if (stack_size != 0 && ra_offset != 0)
                break;

            addr -= 4;
        }
        
        #if BACKTRACE_DEBUG
        debugf("backtrace: %s, ra=%p, sp=%p, fp=%p ra_offset=%d, stack_size=%d\n", 
            bt_type == BT_FUNCTION ? "BT_FUNCTION" : (bt_type == BT_EXCEPTION ? "BT_EXCEPTION" : (bt_type == BT_FUNCTION_FRAMEPOINTER ? "BT_FRAMEPOINTER" : "BT_LEAF")),
            ra, sp, fp, ra_offset, stack_size);
        #endif

        switch (bt_type) {
            case BT_FUNCTION_FRAMEPOINTER:
                if (!fp_offset) {
                    debugf("backtrace: framepointer used but not saved onto stack at %p\n", buffer[i]);
                } else {
                    // Use the frame pointer to refer to the current frame.
                    sp = fp;
                }
                // FALLTRHOUGH!
            case BT_FUNCTION:
                if (fp_offset)
                    fp = *(uint32_t**)((uint32_t)sp + fp_offset);
                ra = *(uint32_t**)((uint32_t)sp + ra_offset) - 2;
                sp = (uint32_t*)((uint32_t)sp + stack_size);
                interrupt_ra = NULL;
                interrupt_rafunc_addr = 0;
                break;
            case BT_EXCEPTION: {
                // Exception frame. We must return back to EPC, but let's keep the
                // RA value. If the interrupted function is a leaf function, we
                // will need it to further walk back.
                // Notice that FP is a callee-saved register so we don't need to
                // recover it from the exception frame (also, it isn't saved there
                // during interrupts).
                interrupt_ra = *(uint32_t**)((uint32_t)sp + ra_offset);

                // Read EPC from exception frame and adjust it with CAUSE BD bit
                ra = *(uint32_t**)((uint32_t)sp + offsetof(reg_block_t, epc) + 32);
                uint32_t cause = *(uint32_t*)((uint32_t)sp + offsetof(reg_block_t, cr) + 32);
                if (cause & C0_CAUSE_BD) ra++;

                sp = (uint32_t*)((uint32_t)sp + stack_size);

                // The next frame might be a leaf function, for which we will not be able
                // to find a stack frame. Try to open the symbol table: if we find it,
                // we can search for the start address of the function so that we know where to
                // stop.
                symtable_header_t symt = symt_open();
                if (symt.head[0]) {
                    int idx; addrtable_entry_t entry;
                    symt_addrtab_search(&symt, (uint32_t)ra, &idx);
                    do {
                        entry = symt_addrtab_entry(&symt, --idx);
                    } while (!ADDRENTRY_IS_FUNC(entry));
                    interrupt_rafunc_addr = ADDRENTRY_ADDR(entry);
                    #if BACKTRACE_DEBUG
                    debugf("Found interrupted function start address: %08lx\n", interrupt_rafunc_addr);
                    #endif
                }
            }   break;
            case BT_LEAF:
                ra = interrupt_ra - 2;
                interrupt_ra = NULL;
                interrupt_rafunc_addr = 0;
                break;
        }
    }

	return size;
}

#define MAX_FILE_LEN 120
#define MAX_FUNC_LEN 120
#define MAX_SYM_LEN  (MAX_FILE_LEN + MAX_FUNC_LEN + 24)

static void format_entry(void (*cb)(void *, backtrace_frame_t *), void *cb_arg, 
    uint32_t SYMTAB_ROM, uint32_t STRTAB_ROM, int idx, uint32_t addr, uint32_t offset, bool is_inline)
{       
    symtable_entry_t s alignas(8);

    data_cache_hit_writeback_invalidate(&s, sizeof(s));
    dma_read(&s, SYMTAB_ROM + idx * sizeof(symtable_entry_t), sizeof(s));

    char file_buf[MAX_FILE_LEN+2] alignas(8);
    char func_buf[MAX_FUNC_LEN+2] alignas(8);

    char *func = func_buf;
    char *file = file_buf;
    if (s.func_sidx & 1) func++;
    if (s.file_sidx & 1) file++;

    int func_len = MIN(s.func_len, MAX_FUNC_LEN);
    int file_len = MIN(s.file_len, MAX_FILE_LEN);

    if (addr >= (uint32_t)inthandler && addr < (uint32_t)inthandler_end) {
        // Special case exception handlers. This is just to show something slightly
        // more readable instead of "notcart+0x0" or similar assembly symbols
        snprintf(func, sizeof(func_buf), "<EXCEPTION HANDLER>");
    } else {
        data_cache_hit_writeback_invalidate(func_buf, sizeof(func_buf));
        dma_read(func, STRTAB_ROM + s.func_sidx, func_len);
        func[func_len] = 0;
    }

    data_cache_hit_writeback_invalidate(file_buf, sizeof(file_buf));
    dma_read(file, STRTAB_ROM + s.file_sidx, MIN(s.file_len, file_len));
    file[file_len] = 0;

    cb(cb_arg, &(backtrace_frame_t){
        .addr = addr,
        .func_offset = offset ? offset : s.func_off,
        .func = func,
        .source_file = file,
        .source_line = s.line,
        .is_inline = is_inline,
    });
}

bool backtrace_symbols_cb(void **buffer, int size, uint32_t flags,
    void (*cb)(void *, backtrace_frame_t *), void *cb_arg)
{
    // Open the symbol table. If not found, abort as we can't symbolize anything.
    symtable_header_t symt_header = symt_open();
    if (!symt_header.head[0]) return false;

    uint32_t SYMTAB_ROM = SYMT_ROM + symt_header.symtab_off;
    uint32_t STRTAB_ROM = SYMT_ROM + symt_header.strtab_off;

    for (int i=0; i<size; i++) {
        int l=0, r=symt_header.symtab_size-1;
        uint32_t needle = (uint32_t)buffer[i];
        while (l <= r) {
            int m = (l+r)/2;
            addrtable_entry_t a = symt_addrtab_entry(&symt_header, m);

            if (ADDRENTRY_ADDR(a) == needle) {
                // We need to format all inlines for this address (if any)
                while (ADDRENTRY_IS_INLINE(a))
                    a = symt_addrtab_entry(&symt_header, --m);
                do {
                    format_entry(cb, cb_arg, SYMTAB_ROM, STRTAB_ROM, m, needle, 0, ADDRENTRY_IS_INLINE(a));
                    a = symt_addrtab_entry(&symt_header, ++m);
                } while (ADDRENTRY_IS_INLINE(a));
                break;
            } else if (ADDRENTRY_ADDR(a) < needle) {
                l = m+1;
            } else {
                r = m-1;
            }
        }

        if (l > r) {
            // We couldn'd find the proper symbol; try to find the function it belongs to
            addrtable_entry_t a;
            for (l--; l>=0; l--) {
                a = symt_addrtab_entry(&symt_header, l);
                if (ADDRENTRY_IS_FUNC(a))
                    break;
            }
            if (l >= 0) {
                format_entry(cb, cb_arg, SYMTAB_ROM, STRTAB_ROM, l, needle, needle - ADDRENTRY_ADDR(a), ADDRENTRY_IS_INLINE(a));
            } else {
                cb(cb_arg, &(backtrace_frame_t){
                    .addr = needle,
                    .func_offset = 0,
                    .func = "???",
                    .source_file = "???",
                    .source_line = 0,
                    .is_inline = 0,
                });
            }
        }
    }
    return true;
}

char** backtrace_symbols(void **buffer, int size)
{
    char **syms = malloc(5 * size * (sizeof(char*) + MAX_SYM_LEN));
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
