#include <stdint.h>
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "n64sys.h"
#include "dma.h"
#include "utils.h"
#include "exception.h"
#include "rompak_internal.h"

/** @brief Symbol table file header */
typedef struct alignas(8) {
    char head[4];           ///< Magic ID "SYMT"
    uint32_t version;       ///< Version of the symbol table
    uint32_t addrtab_off;   ///< Offset of the address table in the file
    uint32_t addrtab_size;  ///< Size of the address table in the file (number of entries)
    uint32_t symtab_off;    ///< Offset of the symbol table in the file
    uint32_t symtab_size;   ///< Size of the symbol table in the file (number of entries)
    uint32_t strtab_off;    ///< Offset of the string table in the file
    uint32_t strtab_size;   ///< Size of the string table in the file (number of entries)
} symtable_header_t;

/** @brief Symbol table entry */
typedef struct {
    uint16_t func_sidx;     ///< Offset of the function name in the string table
    uint16_t func_len;      ///< Length of the function name
    uint16_t file_sidx;     ///< Offset of the file name in the string table
    uint16_t file_len;      ///< Length of the file name
    uint16_t line;          ///< Line number (or 0 if this symbol generically refers to a whole function)
    uint16_t func_off;      ///< Offset of the symbol within its function
} symtable_entry_t;

typedef uint32_t addrtable_entry_t;

#define ADDRENTRY_ADDR(e)       ((e) & ~3)
#define ADDRENTRY_IS_FUNC(e)    ((e) &  1)
#define ADDRENTRY_IS_INLINE(e)  ((e) &  2)

#define MIPS_OP_ADDIU_SP(op)  (((op) & 0xFFFF0000) == 0x27BD0000)
#define MIPS_OP_JR_RA(op)     (((op) & 0xFFFF0000) == 0x03E00008)
#define MIPS_OP_SD_RA_SP(op)  (((op) & 0xFFFF0000) == 0xFFBF0000)
#define MIPS_OP_LUI_GP(op)    (((op) & 0xFFFF0000) == 0x3C1C0000)

#define ABS(x)   ((x) < 0 ? -(x) : (x))

int backtrace(void **buffer, int size)
{
    uint32_t *sp, *ra;
    asm volatile (
        "move %0, $ra\n"
        "move %1, $sp\n"
        : "=r"(ra), "=r"(sp)
    );

    int stack_size = 0;
    for (uint32_t *addr = (uint32_t*)backtrace; !stack_size; ++addr) {
        uint32_t op = *addr;
        if (MIPS_OP_ADDIU_SP(op))
            stack_size = ABS((int16_t)(op & 0xFFFF));
        else if (MIPS_OP_JR_RA(op))
            break;
    }

	// debugf("Start backtrace\n");
    extern uint32_t inthandler[], inthandler_end[];

    sp = (uint32_t*)((uint32_t)sp + stack_size);
    for (int i=0; i<size; ++i) {
        debugf("PC: %p (SP: %p)\n", ra, sp);
        buffer[i] = ra;

        int ra_offset = 0, stack_size = 0;
        for (uint32_t *addr = ra; !ra_offset || !stack_size; --addr) {
            assertf((uint32_t)addr > 0x80000400, "backtrace: invalid address %p", addr);
            uint32_t op = *addr;
            if (MIPS_OP_ADDIU_SP(op)) {
                stack_size = ABS((int16_t)(op & 0xFFFF));
                if (addr >= inthandler && addr < inthandler_end) {
                    ra_offset = offsetof(reg_block_t, epc) + 32;
                    debugf("EXCEPTION HANDLER %d\n", ra_offset);
                }
            } else if (MIPS_OP_SD_RA_SP(op))
                ra_offset = (int16_t)(op & 0xFFFF) + 4; // +4 = load low 32 bit of RA
            else if (MIPS_OP_LUI_GP(op)) { // _start function loads gp, so it's useless to go back more
				// debugf("_start reached, aborting backtrace\n");
                return i;
			}
        }

        ra = *(uint32_t**)((uint32_t)sp + ra_offset);
        sp = (uint32_t*)((uint32_t)sp + stack_size);
    }

	return size;
}

#define MAX_FILE_LEN 60
#define MAX_FUNC_LEN 60
#define MAX_SYM_LEN  (MAX_FILE_LEN + MAX_FUNC_LEN + 24)

int format_entry(char *out, uint32_t SYMTAB_ROM, uint32_t STRTAB_ROM, int idx, uint32_t addr, uint32_t offset)
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

    data_cache_hit_writeback_invalidate(func_buf, sizeof(func_buf));
    dma_read(func, STRTAB_ROM + s.func_sidx, func_len);
    func[func_len] = 0;

    data_cache_hit_writeback_invalidate(file_buf, sizeof(file_buf));
    dma_read(file, STRTAB_ROM + s.file_sidx, MIN(s.file_len, file_len));
    file[file_len] = 0;

    return snprintf(out, MAX_SYM_LEN, "%s+0x%lx (%s:%d) [0x%08lx]", func, offset ? offset : s.func_off, file, s.line, addr);
}

uint32_t addrtab_entry(uint32_t ADDRTAB_ROM, int idx)
{
    return io_read(ADDRTAB_ROM + idx * 4);
}

char** backtrace_symbols(void **buffer, int size)
{
    static uint32_t SYMT_ROM = 0xFFFFFFFF;
    if (SYMT_ROM == 0xFFFFFFFF) {
        SYMT_ROM = rompak_search_ext(".sym");
        if (!SYMT_ROM)
            debugf("backtrace_symbols: no symbol table found in the rompak\n");
    }

    if (!SYMT_ROM) {
        return NULL;
    }

    symtable_header_t symt_header;
    data_cache_hit_writeback_invalidate(&symt_header, sizeof(symt_header));
    dma_read_raw_async(&symt_header, SYMT_ROM, sizeof(symtable_header_t));
    dma_wait();

    if (symt_header.head[0] != 'S' || symt_header.head[1] != 'Y' || symt_header.head[2] != 'M' || symt_header.head[3] != 'T') {
        debugf("backtrace_symbols: invalid symbol table found at 0x%08lx\n", SYMT_ROM);
        return NULL;
    }

    char **syms = malloc(5 * size * (sizeof(char*) + MAX_SYM_LEN));    
    uint32_t SYMTAB_ROM = SYMT_ROM + symt_header.symtab_off;
    uint32_t STRTAB_ROM = SYMT_ROM + symt_header.strtab_off;
    uint32_t ADDRTAB_ROM = SYMT_ROM + symt_header.addrtab_off;

    char *out = (char*)syms + size*sizeof(char*);
    for (int i=0; i<size; i++) {
        syms[i] = out;
        *out = 0;

        int l=0, r=symt_header.symtab_size-1;
        uint32_t needle = (uint32_t)buffer[i] - 8;
        while (l <= r) {
            int m = (l+r)/2;
            addrtable_entry_t a = addrtab_entry(ADDRTAB_ROM, m);

            if (ADDRENTRY_ADDR(a) == needle) {
                // We need to format all inlines for this address (if any)
                while (ADDRENTRY_IS_INLINE(a))
                    a = addrtab_entry(ADDRTAB_ROM, --m);
                do {
                    out += format_entry(out, SYMTAB_ROM, STRTAB_ROM, m, needle, 0) + 1;
                    out[-1] = '\n';
                } while (ADDRENTRY_IS_INLINE(addrtab_entry(ADDRTAB_ROM, ++m)));
                out[-1] = 0;
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
                a = addrtab_entry(ADDRTAB_ROM, l);
                if (ADDRENTRY_IS_FUNC(a))
                    break;
            }
            if (l >= 0) {
                out += format_entry(out, SYMTAB_ROM, STRTAB_ROM, l, needle, needle - ADDRENTRY_ADDR(a)) + 1;
            } else
                out = stpcpy(out, "???") + 1;
        }
    }

    return syms;
}
