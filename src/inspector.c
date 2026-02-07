/**
 * @file inspector.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef NDEBUG
#include "graphics.h"
#include "vi_internal.h"
#include "display.h"
#include "debug.h"
#include "surface.h"
#include "joypad.h"
#include "rompak_internal.h"
#include "joybus/joypad_internal.h"
#include "exception_internal.h"
#include "system.h"
#include "utils.h"
#include "backtrace.h"
#include "backtrace_internal.h"
#include "kernel/kernel_internal.h"
#include "cop0.h"
#include "n64sys.h"
#include "mi.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/** @brief The mode of the inspector (what caused it to be triggered). */
enum Mode {
    MODE_EXCEPTION,
    MODE_ASSERTION,
    MODE_CPP_EXCEPTION
};

enum {
    XTITLE = 64,
    YTITLE = 2,
    XSTART = 48,
    XEND = 640-48,
    YSTART = 16,
    YEND = 240-8-8,
};

/** @brief Pack a 16-bit color into a 32-bit word. */
#define pack32(x16)        ((x16) | ((x16) << 16))

// Colors come from the Solarized color scheme
/// @cond
#define COLOR_BACKGROUND   pack32(color_to_packed16(RGBA32(0x00, 0x2b, 0x36, 255)))
#define COLOR_HIGHLIGHT    pack32(color_to_packed16(RGBA32(0x07, 0x36, 0x42, 128)))
#define COLOR_TEXT         pack32(color_to_packed16(RGBA32(0x83, 0x94, 0x96, 255)))
#define COLOR_EMPHASIS     pack32(color_to_packed16(RGBA32(0x93, 0xa1, 0xa1, 255)))
#define COLOR_ORANGE       pack32(color_to_packed16(RGBA32(0xcb, 0x4b, 0x16, 255)))
#define COLOR_RED          pack32(color_to_packed16(RGBA32(0xdc, 0x32, 0x2f, 255)))
#define COLOR_GREEN        pack32(color_to_packed16(RGBA32(0x2a, 0xa1, 0x98, 255)))
#define COLOR_YELLOW       pack32(color_to_packed16(RGBA32(0xb5, 0x89, 0x00, 255)))
#define COLOR_BLUE         pack32(color_to_packed16(RGBA32(0x26, 0x8b, 0xd2, 255)))
#define COLOR_MAGENTA      pack32(color_to_packed16(RGBA32(0xd3, 0x36, 0x82, 255)))
#define COLOR_CYAN         pack32(color_to_packed16(RGBA32(0x2a, 0xa1, 0x98, 255)))
#define COLOR_WHITE        pack32(color_to_packed16(RGBA32(0xee, 0xe8, 0xd5, 255)))
/// @endcond

static int cursor_x, cursor_y, cursor_columns, cursor_wordwrap;
static enum Mode inspector_mode;
static surface_t *disp;
static int fpr_show_mode = 1;
static int disasm_bt_idx = 0;
static int disasm_max_frames = 0;
static int disasm_offset = 0;
static int thread_offset = 0;
static int num_threads = 0;
static int backtrace_count = 0;

const char *__mips_gpr[34] = {
	"zr", "at", "v0", "v1", "a0", "a1", "a2", "a3",
	"t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
	"s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
	"t8", "t9", "k0", "k1", "gp", "sp", "s8", "ra",
    "lo", "hi"
};

const char *__mips_fpreg[32] = {
    "$f0", "$f1", "$f2", "$f3", "$f4", "$f5", "$f6", "$f7",
    "$f8", "$f9", "$f10", "$f11", "$f12", "$f13", "$f14", "$f15",
    "$f16", "$f17", "$f18", "$f19", "$f20", "$f21", "$f22", "$f23",
    "$f24", "$f25", "$f26", "$f27", "$f28", "$f29", "$f30", "$f31"
};

__attribute__((used))
static void mips_disasm(uint32_t *ptr, char *out, int n) {
	static const char *ops[64] = { 
		"s", "r", "jj", "jjal", "bbeq", "bbne", "bblez", "bbgtz",
		"iaddi", "iaddiu", "islti", "isltiu", "iandi", "iori", "ixori", "klui",
		"qcop0", "fcop1", "ccop2", "ccop3", "bbeql", "bbnel", "bblezl", "bbgtzl",
		"idaddi", "idaddiu", "mldl", "mldr", "*", "*", "*", "*",
		"mlb", "mlh", "mlwl", "mlw", "mlbu", "mlhu", "mlwr", "mlwu",
		"msb", "msh", "mswl", "msw", "msdl", "msdr", "mswr", "*",
		"mll", "nlwc1", "mlwc2", "*", "mlld", "nldc1", "mldc2", "mld",
		"msc", "nswc1", "mswc2", "*", "mscd", "nsdc1", "msdc2", "msd",
  	};
	static const char *special[64]= {
		"esll", "*", "esrl", "esra", "rsllv", "*", "rsrlv", "rsrav",
		"wjr", "wjalr", "*", "*", "asyscall", "abreak", "*", "_sync",
		"cmfhi", "wmthi", "cmflo", "wmtlo", "rdsslv", "*", "rdsrlv", "rdsrav",
		"hmult", "hmultu", "hdiv", "hdivu", "hdmult", "hdmultu", "hddiv", "hddivu", 
		"radd", "raddu", "rsub", "rsubu", "rand", "ror", "rxor", "rnor", 
		"*", "*", "rslt", "rsltu", "rdadd", "rdaddu", "rdsub", "rdsubu", 
		"ttge", "ttgeu", "ttlt", "ttltu", "tteq", "*", "ttne", "*", 
		"edsll", "*", "edsrl", "edsra", "edsll32", "*", "edsrl32", "edsra32", 
	};
	static const char *fpu_ops[64]= {
        "radd", "rsub", "rmul", "rdiv", "ssqrt", "sabs", "smov", "sneg",
        "sround.l", "strunc.l", "sceil.l", "sfloor.l", "sround.w", "strunc.w", "sceil.w", "sfloor.w",
        "*", "*", "*", "*", "*", "*", "*", "*",
        "*", "*", "*", "*", "*", "*", "*", "*",
        "scvt.s", "scvt.d", "*", "*", "scvt.w", "scvt.l", "*", "*",
		"*", "*", "*", "*", "*", "*", "*", "*", 
		"hc.f", "hc.un", "hc.eq", "hc.ueq", "hc.olt", "hc.ult", "hc.ole", "hc.ule", 
		"hc.sf", "hc.ngle", "hc.seq", "hc.ngl", "hc.lt", "hc.nge", "hc.le", "hc.ngt", 
    };
    static const char *cop0_regname[32] = {
        "Index", "Random", "EntryLo0", "EntryLo1", "Context", "PageMask", "Wired", "Reserved7",
        "BadVAddr", "Count", "EntryHi", "Compare", "Status", "Cause", "EPC", "PRId",
        "Config", "LLAddr", "WatchLo", "WatchHi", "XContext", "Reserved21", "Reserved22", "Reserved23",
        "Reserved24", "Reserved25", "ParityErr", "CacheErr", "TagLo", "TagHi", "ErrorEPC", "Reserved31"
    };

	char symbuf[64];

	// Disassemble MIPS instruction
	uint32_t pc = (uint32_t)ptr;
	uint32_t op = *ptr;
	int16_t imm16 = op & 0xFFFF;
	uint32_t tgt16 = (pc + 4) + (imm16 << 2);
	uint32_t imm26 = op & 0x3FFFFFF;
	uint32_t tgt26 = ((pc + 4) & 0xfffffffff0000000) | (imm26 << 2);
	const char *rs = __mips_gpr[(op >> 21) & 0x1F];
	const char *rt = __mips_gpr[(op >> 16) & 0x1F];
	const char *rd = __mips_gpr[(op >> 11) & 0x1F];
	const char *opn = ops[(op >> 26) & 0x3F];
	if (op == 0) opn = "znop";
	else if (((op >> 26) & 0x3F) == 9 && ((op >> 21) & 0x1F) == 0) opn = "kli";
	else if ((op >> 16) == 0x1000) opn = "yb";
	else if (*opn == 's') {
		opn = special[(op >> 0) & 0x3F];
		if (((op >> 0) & 0x3F) == 0x25 && ((op >> 16) & 0x1F) == 0) opn = "smove";
	} else if (*opn == 'f') {
        uint32_t sub = (op >> 21) & 0x1F;
        switch (sub) {
            case 0: opn = "gmfc1"; break;
            case 1: opn = "gdmfc1"; break;
            case 4: opn = "gmtc1"; break;
            case 5: opn = "gdmtc1"; break;
            case 8: switch ((op >> 16) & 0x1F) {
                case 0: opn = "ybc1f"; break;
                case 2: opn = "ybc1fl"; break;
                case 1: opn = "ybc1t"; break;
                case 3: opn = "ybc1tl"; break;
            } break;
            case 16: case 17: case 20: case 21:
                opn = fpu_ops[(op >> 0) & 0x3F];
                sprintf(symbuf, "%s.%s", opn, (sub == 16) ? "s" : (sub == 17) ? "d" : (sub == 20) ? "w" : "l");
                opn = symbuf;
                rt = __mips_fpreg[(op >> 16) & 0x1F];
                rs = __mips_fpreg[(op >> 11) & 0x1F];
                rd = __mips_fpreg[(op >> 6) & 0x1F];
                break;
        }
	} else if (*opn == 'q') { // cop0
        if (((op >> 25) & 1) == 0) {
            uint32_t sub = (op >> 21) & 0xF;
            switch (sub) {
                case 0: opn = "hmfc0"; break;
                case 1: opn = "hdmfc0"; break;
                case 4: opn = "hmtc0"; break;
                case 5: opn = "hdmtc0"; break;
            }
            rs = __mips_gpr[(op >> 16) & 0x1F];
            rt = cop0_regname[(op >> 11) & 0x1F];
        } else {
            switch (op & 0x3F) {
                case 1: opn = "ztlbr"; break;
                case 2: opn = "ztlbwi"; break;
                case 6: opn = "ztlbwr"; break;
                case 8: opn = "ztlbp"; break;
                case 24: opn = "zeret"; break;
            }
        }
    }

	switch (*opn) {
	/* op tgt26 */        case 'j': snprintf(out, n, "%08lx: \aG%-9s \aY%08lx <%s>", pc, opn+1, tgt26, __symbolize((void*)tgt26, symbuf, sizeof(symbuf))); break;
	/* op rt, rs, imm */  case 'i': snprintf(out, n, "%08lx: \aG%-9s \aY%s, %s, %d", pc, opn+1, rt, rs, (int16_t)op); break;
	/* op rt, imm */      case 'k': snprintf(out, n, "%08lx: \aG%-9s \aY%s, %d", pc, opn+1, rt, (int16_t)op); break;
	/* op rt, imm(rs) */  case 'm': snprintf(out, n, "%08lx: \aG%-9s \aY%s, %d(%s)", pc, opn+1, rt, (int16_t)op, rs); break;
	/* op fd, imm(rs) */  case 'n': snprintf(out, n, "%08lx: \aG%-9s \aY%s, %d(%s)", pc, opn+1, __mips_fpreg[(op >> 16) & 0x1F], (int16_t)op, rs); break;
	/* op rd, rs, rt  */  case 'r': snprintf(out, n, "%08lx: \aG%-9s \aY%s, %s, %s", pc, opn+1, rd, rs, rt); break;
	/* op rd, rs */       case 's': snprintf(out, n, "%08lx: \aG%-9s \aY%s, %s", pc, opn+1, rd, rs); break;
	/* op rd, rt, sa  */  case 'e': snprintf(out, n, "%08lx: \aG%-9s \aY%s, %s, %ld", pc, opn+1, rd, rt, (op >> 6) & 0x1F); break;
	/* op rs, rt, tgt16 */case 'b': snprintf(out, n, "%08lx: \aG%-9s \aY%s, %s, %08lx <%s>", pc, opn+1, rs, rt, tgt16, __symbolize((void*)tgt16, symbuf, sizeof(symbuf))); break;
	/* op tgt16 */        case 'y': snprintf(out, n, "%08lx: \aG%-9s \aY%08lx <%s>", pc, opn+1, tgt16, __symbolize((void*)tgt16, symbuf, sizeof(symbuf))); break;
	/* op rs */           case 'w': snprintf(out, n, "%08lx: \aG%-9s \aY%s", pc, opn+1, rs); break;
	/* op rd */           case 'c': snprintf(out, n, "%08lx: \aG%-9s \aY%s", pc, opn+1, rd); break;
	/* op */			  case 'z': snprintf(out, n, "%08lx: \aG%-9s", pc, opn+1); break;
    /* op fd, fs, ft */   case 'f': snprintf(out, n, "%08lx: \aG%-9s \aY%s, %s, %s", pc, opn+1, rd, rs, rt); break;
    /* op rt, fs */       case 'g': snprintf(out, n, "%08lx: \aG%-9s \aY%s, %s", pc, opn+1, rt, __mips_fpreg[(op >> 11) & 0x1F]); break;
	/* op rs, rt */       case 'h': snprintf(out, n, "%08lx: \aG%-9s \aY%s, %s", pc, opn+1, rs, rt); break;
	/* op code20 */ 	  case 'a': snprintf(out, n, "%08lx: \aG%-9s \aY0x%lx", pc, opn+1, (op>>6) & 0xFFFFF); break;
    /* op rs, rt, code */ case 't': snprintf(out, n, "%08lx: \aG%-9s \aY%s, %s, 0x%lx", pc, opn+1, rs, rt, (op>>6) & 0x3FF); break;
					      default:  snprintf(out, n, "%08lx: \aG%-9s", pc, opn+1); break;
	}
}

static bool disasm_valid_pc(uint32_t pc) {
    // TODO: handle TLB ranges?
    return pc >= 0x80000000 && pc < 0x80800000 && (pc & 3) == 0;
}

static int inspector_stdout(char *buf, unsigned int len) {
    for (int i=0; i<len; i++) {
        if (cursor_x >= 640) break;

        switch (buf[i]) {
        case '\a': {
            uint32_t color = COLOR_TEXT;
            switch (buf[++i]) {
            case 'T': color = COLOR_TEXT; break;
            case 'E': color = COLOR_EMPHASIS; break;
            case 'O': color = COLOR_ORANGE; break;
            case 'Y': color = COLOR_YELLOW; break;
            case 'M': color = COLOR_MAGENTA; break;
            case 'G': color = COLOR_GREEN; break;
            case 'W': color = COLOR_WHITE; break;
            }
            graphics_set_color(color, COLOR_BACKGROUND);
        }   break;
        case '\b':
            cursor_wordwrap = true;
            break;
        case '\t':
            cursor_x = ROUND_UP(cursor_x+1, cursor_columns);
            if (cursor_wordwrap && cursor_x >= XEND) {
                cursor_x = XSTART;
                cursor_y += 8;
            }
            break;
        case '\n':
            cursor_x = XSTART;
            if (cursor_y == YTITLE) {
                cursor_y = YSTART;
            } else {
                cursor_y += 8;
            }
            cursor_wordwrap = false;
            graphics_set_color(COLOR_TEXT, COLOR_BACKGROUND);
            break;
        default:
            if (cursor_x < XEND) {
                graphics_draw_character(disp, cursor_x, cursor_y, buf[i]);
                cursor_x += 8;
                if (cursor_wordwrap && buf[i] == ' ') {
                    // Check if we can fit the next word
                    int j = i+1;
                    while (j < len && buf[j] != ' ' && buf[j] != '\n')
                        j++;
                    // If it doesn't fit, wrap
                    if (cursor_x + (j-i)*8 >= XEND) {
                        cursor_x = XSTART;
                        cursor_y += 8;
                    }
                }
                if (cursor_wordwrap && cursor_x >= XEND) {
                    cursor_x = XSTART;
                    cursor_y += 8;
                }
            }
            break;
        }
	}
    return len;
}

static void inspector_print_backtrace(void *bt, int n, int bt_skip, bool also_debugf)
{
    printf("\aWBacktrace:\n");
    if (also_debugf) debugf("Backtrace:\n");
    char func[128];
    bool skip = true;
    void cb(void *arg, backtrace_frame_t *frame) {
        if (also_debugf) { debugf("    "); backtrace_frame_print(frame, stderr); debugf("\n"); }
        if (skip) {
            if (strstr(frame->func, "<EXCEPTION HANDLER>"))
                skip = false;
            return;
        }
        if (bt_skip > 0) {
            bt_skip--;
            return;
        }
        printf("    ");
        snprintf(func, sizeof(func), "\aG%s\aT", frame->func);
        frame->func = func;
        backtrace_frame_print_compact(frame, stdout, 60);
    }
    backtrace_symbols_cb(bt, n, 0, cb, NULL);
    if (skip) {
        // we didn't find the exception handler for some reason (eg: missing symbols)
        // so just print the whole thing
        skip = false;
        backtrace_symbols_cb(bt, n, 0, cb, NULL);
    }
}

static void inspector_page_exception(surface_t *disp, exception_t* ex, joypad_buttons_t *key_pressed) {
    int bt_skip = 0;

    switch (inspector_mode) {
    case MODE_EXCEPTION:
        printf("CPU Exception\n");
        printf("\aO");
        __exception_dump_header(stdout, ex);
        printf("\n");

        printf("\aWInstruction:\n");
        uint32_t epc = (uint32_t)(ex->regs->epc + ((ex->regs->cr & C0_CAUSE_BD) ? 4 : 0));
        if (disasm_valid_pc(epc)) {
            char buf[128];
            mips_disasm((void*)epc, buf, 128);
            printf("    %s\n\n", buf);
        } else {
            printf("    <Invalid PC: %08lx>\n\n", epc);
        }

        if (__kernel) {
            printf("\aWThread:\n    %s\n\n", kthread_current()->name);
        }
        break;

    case MODE_ASSERTION: {
        printf("CPU Assertion\n");
        const char *failedexpr = (const char*)(uint32_t)ex->regs->gpr[4];
        const char *msg = (const char*)(uint32_t)ex->regs->gpr[5];
        va_list args = (va_list)(uint32_t)ex->regs->gpr[6];
        if (msg) {
            printf("\b\aOASSERTION FAILED: ");
            vprintf(msg, args);
            printf("\n\n");
            printf("\aWFailed expression:\n");
            printf("    "); printf("\b%s", failedexpr); printf("\n\n");
        } else {
            printf("\b\aOASSERTION FAILED: %s\n\n", failedexpr);
        }
        if (__kernel) {
            printf("\aWThread:\n    %s\n\n", kthread_current()->name);
        }
        bt_skip = 2;
        break;
    }
    case MODE_CPP_EXCEPTION: {
        printf("Uncaught C++ Exception\n");
        const char *exctype = (const char*)(uint32_t)ex->regs->gpr[4];
        const char *what = (const char*)(uint32_t)ex->regs->gpr[5];
        printf("\b\aOC++ Exception: %s\n\n", what);
        if (exctype) {
            printf("\aWException type:\n");
            printf("    "); printf("\b%s", exctype); printf("\n\n");
        }
        if (__kernel) {
            printf("\aWThread:\n    %s\n\n", kthread_current()->name);
        }
        bt_skip = 5;
        break;
    }
    }

    if (backtrace_count++ == 0)
        return;

    void *bt[32];
    int n = backtrace(bt, 32);
    inspector_print_backtrace(bt, n, bt_skip, backtrace_count == 2);
}

static void inspector_page_gpr(surface_t *disp, exception_t* ex, joypad_buttons_t *key_pressed) {
    printf("CPU Registers\n");
    cursor_columns = 92;

    int c = 0;
    void cb(void *arg, const char *name, char *value) {
        printf("\t\aW%s: \aT%s", name, value);
        if (++c % 2 == 0)
            printf("\n");
    }

    __exception_dump_gpr(ex, cb, NULL);
}

static void inspector_page_fpr(surface_t *disp, exception_t* ex, joypad_buttons_t *key_pressed) {
    if (key_pressed->a)
        fpr_show_mode = (fpr_show_mode + 1) % 3;

    printf(fpr_show_mode == 0 ? "CPU Floating Point Registers (Hex)\n" :
           fpr_show_mode == 1 ? "CPU Floating Point Registers (Single)\n" :
                                "CPU Floating Point Registers (Double)");

    int c = 0;
    void cb(void *arg, const char *name, char *hexvalue, char *singlevalue, char *doublevalue) {
        char *value = fpr_show_mode == 0 ? hexvalue : fpr_show_mode == 1 ? singlevalue : doublevalue;
        printf("\t\aW%4s: \aT%-19s%s", name, value, ++c % 2 == 0 ? "\n" : "\t");
    }

    __exception_dump_fpr(ex, cb, NULL);
}

static void inspector_page_disasm(surface_t *disp, exception_t* ex, joypad_buttons_t *key_pressed) {
    if (key_pressed->d_up && disasm_bt_idx > 0) {        
        disasm_bt_idx--;
        disasm_offset = 0;
    }
    if (key_pressed->d_down && disasm_bt_idx < disasm_max_frames-1) {
        disasm_bt_idx++;
        disasm_offset = 0;
    }
    if (key_pressed->c_up) {
        disasm_offset -= 4*6;
    }
    if (key_pressed->c_down) {
        disasm_offset += 4*6;
    }

    printf("Disassembly\n");

	void *bt[32];
	int n = backtrace(bt, 32);

    if (disasm_bt_idx < 2) printf("\n");
    if (disasm_bt_idx < 1) printf("\n");

    bool skip = true;
    uint32_t frame_pc = 0;
    int frame_idx = 0;
    void cb(void *arg, backtrace_frame_t *frame) {
        if (skip) {
            if (strstr(frame->func, "<EXCEPTION HANDLER>"))
                skip = false;
            return;
        }
        if (frame_idx >= disasm_bt_idx-2 && frame_idx <= disasm_bt_idx+2) {
            if (frame_idx == disasm_bt_idx) {
                printf("\aW\t---> ");
                frame_pc = frame->addr;
            }
            else
                printf("\t     ");
            
            const char *basename = strrchr(frame->source_file, '/');
            if (basename) basename++;
            else basename = frame->source_file;
            printf("%08lx %s (%s:%d)\n", frame->addr, frame->func, basename, frame->source_line);
        }
        frame_idx++;
    }
    backtrace_symbols_cb(bt, n, 0, cb, NULL);
    disasm_max_frames = frame_idx;

    if (disasm_bt_idx >= disasm_max_frames-2) printf("\n");
    if (disasm_bt_idx >= disasm_max_frames-1) printf("\n");

    printf("\n\n");

    uint32_t pc = frame_pc + disasm_offset - 9*4;
    char buf[128];
    for (int i=0; i<18; i++) {
        if (!disasm_valid_pc(pc)) {
            printf("\t<invalid address>\n");
        } else {
            mips_disasm((void*)pc, buf, 128);
            if (pc == frame_pc) {
                printf("\aW---> ");
            }
            else
                printf("     ");
            printf("%s\n", buf);
        }
        pc += 4;
    }
}

static void inspector_page_threads(surface_t *disp, exception_t* ex, joypad_buttons_t *key_pressed)
{
    const int THREAD_LEN = 12;
    const int THREADS_PER_LINE = 5;
    kthread_t *th_sel = NULL;

    if (key_pressed->d_left && thread_offset > 0) { thread_offset--; }
    if (key_pressed->d_right && thread_offset < num_threads-1) { thread_offset++; }
    if (key_pressed->d_up && thread_offset >= THREADS_PER_LINE) { thread_offset -= THREADS_PER_LINE; }
    if (key_pressed->d_down && thread_offset+THREADS_PER_LINE < num_threads) { thread_offset += THREADS_PER_LINE; }

    printf("Threads\n");
    if (!__kernel) {
        printf("\aWkernel not initialized\n");
        return;
    }

    int i = 0;
    for (kthread_t *th = __kernel_all_threads; th; th = th->all_next) {
        char buf[THREAD_LEN+4];

        if (i == thread_offset) {
            sprintf(buf, "[%.*s]", THREAD_LEN, th->name);
            printf("\aW%-*s\aT", THREAD_LEN+2, buf);
            th_sel = th;
        } else {
            sprintf(buf, " %.*s ", THREAD_LEN, th->name);
            printf("%-*s", THREAD_LEN+2, buf);
        }
        
        if ((i+1)%THREADS_PER_LINE == 0) printf("\n");
        i++;
    }
    if (i % THREADS_PER_LINE != 0) printf("\n");
    printf("\n");

    char type[64] = {0};
    
    if (th_sel->flags & TH_FLAG_INSPECTOR1) 
        strcat(type, "suspended ");
    if (th_sel->flags & TH_FLAG_DETACHED) {
        strcat(type, "detached ");
        if (th_sel->flags & TH_FLAG_ZOMBIE)
            strcat(type, "zombie ");
    } else if (th_sel->joiner) {
        strcat(type, "joined(");
        strcat(type, th_sel->joiner->name);
        strcat(type, ") ");
    } else {
        strcat(type, "joinable ");
        if (th_sel->flags & TH_FLAG_WAITFORJOIN) 
            strcat(type, "joinable finished ");
    }

    printf("    \aWPriority: \aT%-30d\aWStack size: \aT%d KiB\n", th_sel->pri, th_sel->stack_size / 1024);
    printf("    \aWType: \aT%-34.34s\aWStack: \aT%p\n", type, th_sel->stack);
    printf("    \aWPC: \aT%p\n", th_sel == kthread_current() ? (void*)ex->regs->epc : (void*)th_sel->stack_state->epc);
    printf("\n");

    void *bt[32]; int n;
    n = kthread_backtrace(th_sel, bt, 32);
    inspector_print_backtrace(bt, n, 0, false);
}

static void version_callback(void *ctx, char *key, char *value)
{
    printf("    \aG%s: \aT%s\n", key, value);
}

static bool version_walk(void *ctx, const char *name, pi_addr_t address, size_t size)
{
    if (strstr(name, ".version")) {
        printf("\aW%s:\n", name);
        rompak_version_parse(address, size, version_callback, ctx);
        printf("\n");
    }
    return true;
}

static void inspector_page_version(surface_t *disp, exception_t* ex, joypad_buttons_t *key_pressed)
{
    printf("Versions\n");
    rompak_walk(version_walk, NULL);
}

/** @brief The inspector pages */
inspector_page_t inspector_pages[16] = {
    inspector_page_exception,
    inspector_page_gpr,
    inspector_page_fpr,
    inspector_page_disasm,
    inspector_page_version,
    inspector_page_threads,
};

__attribute__((noreturn))
static void inspector(exception_t* ex, enum Mode mode) {
    static bool in_inspector = false;
    if (in_inspector) abort();
    in_inspector = true;
    inspector_mode = mode;

    if (__kernel) {
        // Reverse the order of the thread list, so that we show the oldest threads
        // first in the list
        kthread_t *prev = NULL;
        kthread_t *th = __kernel_all_threads;
        while (th) {
            kthread_t *next = th->all_next;
            th->all_next = prev;
            prev = th;
            th = next;
        }
        __kernel_all_threads = prev;

        // Suspend all threads but idle. This avoids the inpsector to be interrupted
        // by other threads, and also allows to consistently inspect the state of the threads
        kthread_t *current = kthread_current();
        for (kthread_t *th = __kernel_all_threads; th; th = th->all_next) {
            if (th != current && strcmp(th->name, "idle") != 0) {
                // Copy the current suspended flag into the inspector flag,
                // so that we can later dump the state of the thread correctly
                if (th->flags & TH_FLAG_SUSPENDED)
                    th->flags |= TH_FLAG_INSPECTOR1;
                else
                    th->flags &= ~TH_FLAG_INSPECTOR1;
                kthread_suspend(th);
            }
            ++num_threads;
        }

        // Mask interrupts that we don't need during the inspector, so that
        // their callbacks aren't called, which might cause crashes now.
        // When the kernel is running, interrupts can always be activated in
        // case of thread switch (eg: when using kirq), so to completely
        // disable them we need to mask them.
        *MI_MASK = MI_WMASK_CLR_DP | MI_WMASK_CLR_AI | MI_WMASK_CLR_VI;
    }

    // Call vi_write_end_forced in case we crashed within vi_begin() block,
    // otherwise all VI register changes would not be applied.
    vi_write_end_forced();

    // Close display if it was open. This is useful mainly to free the framebuffers
    // and hopefully be able to allocate the inspector's framebuffers even in
    // low memory conditions.
	display_close();

    // Reset the VI to default state, so that we don't inherit any weird
    // configuration from the crashed program.
    vi_init();
    vi_reset();
	
    // Try to allocate two framebuffers. We might be out of memory though,
    // so be happy with a single framebuffer if that fails.
    int fbidx = 0;
    surface_t fb[2] = {0};
    fb[0] = surface_alloc(FMT_RGBA16, 640, 240);
    if (fb[0].buffer != NULL) {
        fb[1] = surface_alloc(FMT_RGBA16, 640, 240);
    } else {
        // If we couldn't allocate any framebuffer, try to use the memory at
        // the end of RAM. There's no guarantee this won't corrupt the inspector,
        // but at least we try something.
        int mem_size = get_memory_size();
        void *end_mem = (void*)(0x80000000 + mem_size);
        fb[0] = surface_make_linear(end_mem - 64*1024 - 640*240*2, FMT_RGBA16, 640, 240);
    }

	hook_stdio_calls(&(stdio_t){ NULL, inspector_stdout, NULL });

    bool first_frame = true;
    sys_version_t version = {0};
    joypad_buttons_t key_old = {0};
    joypad_buttons_t key_pressed = {0};
    int prevPad = -1;
	int page = 0;
    int page_count = sizeof(inspector_pages) / sizeof(inspector_pages[0]);
    while (inspector_pages[page_count-1] == NULL) {
        page_count--;
    }

	while (1) {
        if (key_pressed.z || key_pressed.r) {
            // Do page wrapping logic from left
            page++;
            if (page >= page_count) page = 0;
        }
        if (key_pressed.l) {
            // Do page wrapping logic from right
            page--;
            if (page < 0) page = page_count - 1;
        }
		disp = &fb[fbidx];

        // Clear the screen, initialize printf cursor position
        cursor_x = XTITLE;
        cursor_y = YTITLE;
        cursor_columns = 8*8;
        graphics_set_color(COLOR_TEXT, COLOR_BACKGROUND);
        graphics_fill_screen(disp, COLOR_BACKGROUND);
        graphics_draw_box(disp, 0, 0, 640, 12, COLOR_TEXT);
        graphics_set_color(COLOR_BACKGROUND, COLOR_TEXT);
    
        // Draw the current page
        inspector_pages[page](disp, ex, &key_pressed);
        fflush(stdout);

        // Draw the footer
        cursor_x = XSTART;
        cursor_y = YEND + 2;
        cursor_columns = 64;
        graphics_draw_box(disp, 0, YEND, 640, 240-YEND, COLOR_TEXT);
        graphics_set_color(COLOR_BACKGROUND, COLOR_TEXT);
        int indent = 10 - (strlen(version.branch) + (version.dirty ? 1 : 0) + strlen(version.commit_date))/2;
        for (int i = 0; i < indent; i++) putc(' ', stdout);
		printf("LibDragon Inspector | %s%s (%s, %.7s) | Page %d/%d", 
            version.branch, version.dirty ? "*" : "", version.commit_date, version.hash,
            page+1, page_count);
        fflush(stdout);

        // Show the screen
		vi_show(disp);
        vi_wait_vblank();
        if (fb[1].buffer != NULL) {
            fbidx ^= 1;
        }

        // If we drew the first frame, we skipped the backtrace to make sure at least
        // basic crash information is shown in case the backtrace crashes.
        // So redraw immediately to attemp displaying the backtrace now.
        if (first_frame) {
            first_frame = false;
            // parse version information once
            sys_get_version(&version);
            continue;
        }

        // Loop until a keypress
        while (1) {
            // Read controller using #joypad_read_n64_inputs, which works also when
            // the interrupts are disabled and when #joypad_init has not been called.
            int keyPress = false;
            for (int i = 0; i < 4; i++) {
                joypad_buttons_t key_new = joypad_read_n64_inputs(i).btn;
                if ((key_new.raw || prevPad == i) && key_new.raw != key_old.raw) {
                    key_pressed = (joypad_buttons_t){ .raw = key_new.raw & ~key_old.raw };
                    key_old = key_new;
                    prevPad = i;
                    keyPress = true;
                    break;
                };
            }
            if (keyPress) {
                break;
            }
            // Avoid constantly banging the PIF with controller reads, that
            // would prevent the RESET button from working.
            wait_ms(1);
        }
    }

	abort();
}

void __inspector_add_page(inspector_page_t page) {
    for (int i = 0; i < sizeof(inspector_pages) / sizeof(inspector_pages[0]); i++) {
        if (inspector_pages[i] == NULL) {
            inspector_pages[i] = page;
            return;
        }
    }
    assertf(0, "Too many inspector pages");
}

__attribute__((noreturn))
void __inspector_exception(exception_t* ex) {
    inspector(ex, MODE_EXCEPTION);
}

__attribute__((noreturn))
void __inspector_assertion(const char *failedexpr, const char *msg, va_list args) {
    asm volatile (
        "move $a0, %0\n"
        "move $a1, %1\n"
        "move $a2, %2\n"
        "syscall 0x1\n"
        :: "p"(failedexpr), "p"(msg), "p"(args)
    );
    __builtin_unreachable();
}

__attribute__((noreturn))
void __inspector_cppexception(const char *exctype, const char *what) {
    asm volatile (
        "move $a0, %0\n"
        "move $a1, %1\n"
        "syscall 0x2\n"
        :: "p"(exctype), "p"(what)
    );
    __builtin_unreachable();    
}

/** @brief Register the inspector as a syscall handler (global constructor run before main). */
__attribute__((constructor))
void __inspector_init(void) {
    // Register SYSCALL 0x1 for assertion failures
    void handler(exception_t* ex, uint32_t code) {
        if (code == 1) inspector(ex, MODE_ASSERTION);
        if (code == 2) inspector(ex, MODE_CPP_EXCEPTION);
    }
    register_syscall_handler(handler, 0x00001, 0x00002);
}
#endif