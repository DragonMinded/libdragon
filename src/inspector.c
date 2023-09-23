#ifndef NDEBUG
#include "graphics.h"
#include "display.h"
#include "debug.h"
#include "joypad.h"
#include "joypad_internal.h"
#include "exception_internal.h"
#include "system.h"
#include "utils.h"
#include "backtrace.h"
#include "backtrace_internal.h"
#include "dlfcn_internal.h"
#include "cop0.h"
#include "n64sys.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

enum Mode {
    MODE_EXCEPTION,
    MODE_ASSERTION,
    MODE_CPP_EXCEPTION
};

enum {
    XSTART = 48,
    XEND = 640-48,
    YSTART = 16,
    YEND = 240-8-8,
};

#define pack32(x16)        ((x16) | ((x16) << 16))

// Colors are coming from the Solarized color scheme
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

static int cursor_x, cursor_y, cursor_columns, cursor_wordwrap;
static surface_t *disp;
static int fpr_show_mode = 1;
static int disasm_bt_idx = 0;
static int disasm_max_frames = 0;
static int disasm_offset = 0;
static int module_offset = 0;
static bool first_backtrace = true;

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
		"iaddi", "iaddiu", "rslt", "isltiu", "iandi", "iori", "ixori", "klui",
		"ccop0", "fcop1", "ccop2", "ccop3", "bbeql", "bbnel", "bblezl", "bbgtzl",
		"ddaddi", "ddaddiu", "dldl", "dldr", "*", "*", "*", "*",
		"mlb", "mlh", "mlwl", "mlw", "mlbu", "mlhu", "mlwr", "mlwu",
		"msb", "msh", "mswl", "msw", "msdl", "msdr", "mswr", "*",
		"mll", "nlwc1", "mlwc2", "*", "mlld", "nldc1", "mldc2", "mld",
		"msc", "nswc1", "mswc2", "*", "mscd", "nsdc1", "msdc2", "msd",
  	};
	static const char *special[64]= {
		"esll", "*", "esrl", "esra", "rsllv", "*", "rsrlv", "rsrav",
		"wjr", "wjalr", "*", "*", "asyscall", "abreak", "*", "_sync",
		"wmfhi", "wmflo", "wmthi", "wmtlo", "rdsslv", "*", "rdsrlv", "rdsrav",
		"*", "*", "*", "*", "*", "*", "*", "*", 
		"radd", "raddu", "rsub", "rsubu", "rand", "ror", "rxor", "rnor", 
		"*", "*", "*", "*", "*", "*", "*", "*", 
		"*", "*", "*", "*", "*", "*", "*", "*", 
		"*", "*", "*", "*", "*", "*", "*", "*", 
	};
	static const char *fpu_ops[64]= {
        "radd", "rsub", "rmul", "rdiv", "rsqrt", "sabs", "smov", "sneg",
        "sround.l", "strunc.l", "sceil.l", "sfloor.l", "sround.w", "strunc.w", "sceil.w", "sfloor.w",
        "*", "*", "*", "*", "*", "*", "*", "*",
        "*", "*", "*", "*", "*", "*", "*", "*",
        "scvt.s", "scvt.d", "*", "*", "scvt.w", "scvt.l", "*", "*",
		"*", "*", "*", "*", "*", "*", "*", "*", 
		"hc.f", "hc.un", "hc.eq", "hc.ueq", "hc.olt", "hc.ult", "hc.ole", "hc.ule", 
		"hc.sf", "hc.ngle", "hc.seq", "hc.ngl", "hc.lt", "hc.nge", "hc.le", "hc.ngt", 
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
            case 16: case 17:
                opn = fpu_ops[(op >> 0) & 0x3F];
                sprintf(symbuf, "%s.%s", opn, (sub == 16) ? "s" : "d");
                opn = symbuf;
                rt = __mips_fpreg[(op >> 11) & 0x1F];
                rs = __mips_fpreg[(op >> 16) & 0x1F];
                rd = __mips_fpreg[(op >> 6) & 0x1F];
                break;
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
	/* op rt */           case 'w': snprintf(out, n, "%08lx: \aG%-9s \aY%s", pc, opn+1, rs); break;
	/* op */			  case 'z': snprintf(out, n, "%08lx: \aG%-9s", pc, opn+1); break;
    /* op fd, fs, ft */   case 'f': snprintf(out, n, "%08lx: \aG%-9s \aY%s, %s, %s", pc, opn+1, rd, rs, rt); break;
    /* op rt, fs */       case 'g': snprintf(out, n, "%08lx: \aG%-9s \aY%s, %s", pc, opn+1, rt, __mips_fpreg[(op >> 11) & 0x1F]); break;
	/* op rt, rs */       case 'h': snprintf(out, n, "%08lx: \aG%-9s \aY%s, %s", pc, opn+1, rt, rs); break;
	/* op code20 */ 	  case 'a': snprintf(out, n, "%08lx: \aG%-9s \aY0x%lx", pc, opn+1, (op>>6) & 0xFFFFF); break;
					      default:  snprintf(out, n, "%08lx: \aG%-9s", pc, opn+1); break;
	}
}

bool disasm_valid_pc(uint32_t pc) {
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
            cursor_y += 8;
            cursor_wordwrap = false;
            graphics_set_color(COLOR_TEXT, COLOR_BACKGROUND);
            break;
        default:
            if (cursor_x < XEND) {
                graphics_draw_character(disp, cursor_x, cursor_y, buf[i]);
                cursor_x += 8;
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

static void title(const char *title) {
    graphics_draw_box(disp, 0, 0, 640, 12, COLOR_TEXT);
    graphics_set_color(COLOR_BACKGROUND, COLOR_TEXT);
    graphics_draw_text(disp, 64, 2, title);
    graphics_set_color(COLOR_TEXT, COLOR_BACKGROUND);
}

static void inspector_page_exception(surface_t *disp, exception_t* ex, enum Mode mode, bool with_backtrace) {
    int bt_skip = 0;

    switch (mode) {
    case MODE_EXCEPTION:
        title("CPU Exception");
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
        break;

    case MODE_ASSERTION: {
        title("CPU Assertion");
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
        bt_skip = 2;
        break;
    }
    case MODE_CPP_EXCEPTION: {
        title("Uncaught C++ Exception");
        const char *exctype = (const char*)(uint32_t)ex->regs->gpr[4];
        const char *what = (const char*)(uint32_t)ex->regs->gpr[5];
        printf("\b\aOC++ Exception: %s\n\n", what);
        if (exctype) {
            printf("\aWException type:\n");
            printf("    "); printf("\b%s", exctype); printf("\n\n");
        }
        bt_skip = 5;
        break;
    }
    }

    if (!with_backtrace)
        return;

    void *bt[32];
    int n = backtrace(bt, 32);

    printf("\aWBacktrace:\n");
    if (first_backtrace) debugf("Backtrace:\n");
    char func[128];
    bool skip = true;
    void cb(void *arg, backtrace_frame_t *frame) {
        if (first_backtrace) { debugf("    "); backtrace_frame_print(frame, stderr); debugf("\n"); }
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
    first_backtrace = false;
}

static void inspector_page_gpr(surface_t *disp, exception_t* ex) {
    title("CPU Registers");
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

    title(fpr_show_mode == 0 ? "CPU Floating Point Registers (Hex)" :
          fpr_show_mode == 1 ? "CPU Floating Point Registers (Single)" :
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

    title("Disassembly");

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

static void inspector_page_modules(surface_t *disp, exception_t* ex, joypad_buttons_t *key_pressed)
{
    dl_module_t *curr_module = __dl_list_head;
    size_t module_idx = 0;
    if(key_pressed->d_up && module_offset > 0) {
        module_offset--;
    }
    if(key_pressed->d_down && module_offset+18 < __dl_num_loaded_modules) {
        module_offset++;
    }
    title("Loaded modules");
    while(curr_module) {
        if(module_idx >= module_offset && module_idx < module_offset+18) {
            void *module_min = curr_module->module->prog_base;
            void *module_max = ((uint8_t *)module_min)+curr_module->module->prog_size;
            printf("%s (%p-%p)\n", curr_module->filename, module_min, module_max);
        }
        curr_module = curr_module->next;
        module_idx++;
    }
}

__attribute__((noreturn))
static void inspector(exception_t* ex, enum Mode mode) {
    static bool in_inspector = false;
    if (in_inspector) abort();
    in_inspector = true;

	display_close();
	display_init(RESOLUTION_640x240, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE);

	enum Page {
		PAGE_EXCEPTION,
		PAGE_GPR,
		PAGE_FPR,
		PAGE_CODE,
        PAGE_MODULES
	};
	enum { PAGE_COUNT = PAGE_MODULES+1 };

	hook_stdio_calls(&(stdio_t){ NULL, inspector_stdout, NULL });

    static bool backtrace = false;
    joypad_buttons_t key_old = {0};
    joypad_buttons_t key_pressed = {0};
	enum Page page = PAGE_EXCEPTION;
	while (1) {
        if (key_pressed.z || key_pressed.r) {
            //Do page wrapping logic from left
            if(page == PAGE_COUNT-1) {
                page = 0;
            } else {
                page++;
            }
        }
        if (key_pressed.l) {
            //Do page wrapping logic from right
            if(page == 0) {
                page = PAGE_COUNT-1;
            } else {
                page--;
            }
        }
		disp = display_get();

        cursor_x = XSTART;
        cursor_y = YSTART;
        cursor_columns = 8*8;
        graphics_set_color(COLOR_TEXT, COLOR_BACKGROUND);
        graphics_fill_screen(disp, COLOR_BACKGROUND);

		switch (page) {
		case PAGE_EXCEPTION:
            inspector_page_exception(disp, ex, mode, backtrace);
			break;
        case PAGE_GPR:
            inspector_page_gpr(disp, ex);
            break;
        case PAGE_FPR:
            inspector_page_fpr(disp, ex, &key_pressed);
            break;
        case PAGE_CODE:
            inspector_page_disasm(disp, ex, &key_pressed);
            break;
            
        case PAGE_MODULES:
            inspector_page_modules(disp, ex, &key_pressed);
            break;
		}

        fflush(stdout);

        cursor_x = XSTART;
        cursor_y = YEND + 2;
        cursor_columns = 64;
        graphics_draw_box(disp, 0, YEND, 640, 240-YEND, COLOR_TEXT);
        graphics_set_color(COLOR_BACKGROUND, COLOR_TEXT);
		printf("\t\t\tLibDragon Inspector | Page %d/%d", page+1, PAGE_COUNT);
        fflush(stdout);

        extern void display_show_force(display_context_t disp);
		display_show_force(disp);

        // Loop until a keypress
        while (1) {
            // Read controller using #joypad_read_n64_inputs, which works also when
            // the interrupts are disabled and when #joypad_init has not been called.
            joypad_buttons_t key_new = joypad_read_n64_inputs(JOYPAD_PORT_1).btn;
            if (key_new.raw != key_old.raw) {
                key_pressed = (joypad_buttons_t){ .raw = key_new.raw & ~key_old.raw };
                key_old = key_new;
	            break;
            };
            // If we draw the first frame, turn on backtrace and redraw immediately
            if (!backtrace) {
                backtrace = true;
                break;
            }
            // Avoid constantly banging the PIF with controller reads, that
            // would prevent the RESET button from working.
            wait_ms(1);
        }
    }

	abort();
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

__attribute__((constructor))
void __inspector_init(void) {
    // Register SYSCALL 0x1 for assertion failures
    void handler(exception_t* ex, uint32_t code) {
        if (code == 1) inspector(ex, MODE_ASSERTION);
        if (code == 2) inspector(ex, MODE_CPP_EXCEPTION);
    }
    register_syscall_handler(handler, 0x00001, 0x00002);
}
#endif /* NDBUEG */
