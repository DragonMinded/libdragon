#define SET_REG(no, value) ({ \
    asm volatile( \
        ".set noat \n" \
        "dli $" #no ", 0x" #value #value #value #value #value #value #value #value "\n" \
        "dmtc1 $" #no ", $f" #no "\n" \
        ".set at \n" \
        :"=X"(dependency) \
        : \
        :"$" #no, "$f" #no \
    ); \
})

#define SET_FP_REG(no, value) ({ \
    asm volatile( \
        "dli $26, 0x" #value #value #value #value #value #value #value #value "\n" \
        "dmtc1 $26, $f" #no "\n" \
        :"=X"(dependency) \
        : \
        :"$26", "$f" #no \
    ); \
})

#define GET_FP_REG(no) ({ \
    asm volatile( \
        "dmfc1 $26, $f" #no " \n" \
        "sd $26,%0 \n" \
        :"=m"(fp_registers_after_ex[no]) \
        : \
        :"$26" \
    ); \
})

#define GET_REG(no, value) ({ \
    asm volatile( \
        ".set noat \n" \
        "sd $" #no ",%0 \n" \
        "dmfc1 $" #no ", $f" #no "\n" \
        "sd $" #no ",%1 \n" \
        ".set at \n" \
        :"=m"(registers_after_ex[no]), "=m"(fp_registers_after_ex[no]) \
        : \
        :"$" #no \
    ); \
})

#define ASSERT_REG_FP(no, value) ({ \
    ASSERT_EQUAL_HEX(fp_registers_after_ex[no], 0x##value##value##value##value##value##value##value##value, "$f" #no " not saved"); \
})

#define ASSERT_REG_GP(no, value) ({ \
    if (no != 0) \
        ASSERT_EQUAL_HEX(registers_after_ex[no], 0x##value##value##value##value##value##value##value##value, "$" #no " not saved"); \
})

#define ASSERT_REG_FP_HANDLER(no, value) ({ \
    ASSERT_EQUAL_HEX(exception_regs.fpr[no], 0x##value##value##value##value##value##value##value##value, "$f" #no " not available to the handler"); \
})

#define ASSERT_REG_GP_HANDLER(no, value) ({ \
    if (no != 0) \
        ASSERT_EQUAL_HEX(exception_regs.gpr[no], 0x##value##value##value##value##value##value##value##value, "$" #no " not available to the handler"); \
})

#define ASSERT_REG(no, value) ({ \
    ASSERT_REG_FP(no, value); \
    ASSERT_REG_GP(no, value); \
    ASSERT_REG_FP_HANDLER(no, value); \
    ASSERT_REG_GP_HANDLER(no, value); \
})

void test_exception(TestContext *ctx) {
    // Bring FCR31 to a known state as some fp operations setting the inexact op flag
    uint32_t known_fcr31 = C1_FCR31();
    C1_WRITE_FCR31(0);
    DEFER(C1_WRITE_FCR31(known_fcr31));

    uint64_t registers_after_ex[32];
    uint64_t fp_registers_after_ex[32];
    uint64_t lo, hi;
    volatile int exception_occurred = 0;
    reg_block_t exception_regs;

    // This is only used to make sure we break after setting all the registers
    uint32_t dependency;

    void ex_handler(exception_t* ex) {
        // Fill as many regs as possible with invalid values
        SET_REG(0,  A0);
        SET_REG(1,  A1);
        SET_REG(2,  A2);
        SET_REG(3,  A3);
        SET_REG(4,  A4);
        SET_REG(5,  A5);
        SET_REG(6,  A6);
        SET_REG(7,  A7);
        SET_REG(8,  A8);
        SET_REG(9,  A9);
        SET_REG(10, B0);
        SET_REG(11, B1);
        SET_REG(12, B2);
        SET_REG(13, B3);
        SET_REG(14, B4);
        SET_REG(15, B5);
        SET_REG(16, B6);
        SET_REG(17, B7);
        SET_REG(18, B8);
        SET_REG(19, B9);
        SET_REG(20, C0);
        SET_REG(21, C1);
        SET_REG(22, C2);
        SET_REG(23, C3);
        SET_REG(24, C4);
        SET_REG(25, C5);

        // We cannot set 28 & 29 via inline assembly

        SET_FP_REG(26, C6);
        SET_FP_REG(27, C7);
        SET_FP_REG(28, C8);
        SET_FP_REG(29, C9);

        SET_REG(30, D0);
        SET_REG(31, D1);

        exception_regs = *ex->regs;

        switch(ex->code) {
            case EXCEPTION_CODE_TLB_LOAD_I_MISS:
                exception_occurred++;
                ex->regs->epc += 4;
            break;
            default:
                exception_default_handler(ex);
            break;
        };
    }

    register_exception_handler(ex_handler);
    DEFER(register_exception_handler(exception_default_handler));

    ASSERT_EQUAL_SIGNED(exception_occurred, 0, "Exception triggered early");

    // Set as many registers as possible to known values before the ex.
    SET_REG(0, 00);
    SET_REG(1, 01);
    SET_REG(2, 02);
    SET_REG(3, 03);
    SET_REG(4, 04);
    SET_REG(5, 05);
    SET_REG(6, 06);
    SET_REG(7, 07);
    SET_REG(8, 08);
    SET_REG(9, 09);
    SET_REG(10, 10);
    SET_REG(11, 11);
    SET_REG(12, 12);
    SET_REG(13, 13);
    SET_REG(14, 14);
    SET_REG(15, 15);
    SET_REG(16, 16);
    SET_REG(17, 17);
    SET_REG(18, 18);
    SET_REG(19, 19);
    SET_REG(20, 20);
    SET_REG(21, 21);
    SET_REG(22, 22);
    SET_REG(23, 23);
    SET_REG(24, 24);
    SET_REG(25, 25);

    // Cannot set 28 & 29 as they are used by GCC and it complains about it
    // instead get the current sp & gp. This will at least make sure the
    // inthandler is not modifying them on its own
    uint64_t gp;
    asm volatile("sd $28,%0":"=m"(gp), "=X"(dependency));
    uint64_t sp;
    asm volatile("sd $29,%0":"=m"(sp), "=X"(dependency));

    // Set fp registers 26-29 independent of GP registers
    // as the SET_REG macro will try to manipulate sp & gp
    SET_FP_REG(26, 26);
    SET_FP_REG(27, 27);
    SET_FP_REG(28, 28);
    SET_FP_REG(29, 29);

    SET_REG(30, 30);
    SET_REG(31, 31);

    // Set lo & hi
    asm volatile(
        "dli $26, 0xDEADBEEFDEADBEEF \n"
        "mtlo $26 \n"
        "dli $26, 0xBEEFF00DBEEFF00D \n"
        "mthi $26 \n"
        :"=X"(dependency)
        :
        :"$26", "lo", "hi"
    );

    // Make sure we trigger the exception after setting all the registers
    // dependency may not be necessary in practice but let's inform GCC
    // just in case.
    extern const void test_exception_opcode;
    asm volatile("test_exception_opcode: lw $0,0($0)" ::"X"(dependency));

    // Read all registers to mem at once
    GET_REG(0,  00);
    GET_REG(1,  01);
    GET_REG(2,  02);
    GET_REG(3,  03);
    GET_REG(4,  04);
    GET_REG(5,  05);
    GET_REG(6,  06);
    GET_REG(7,  07);
    GET_REG(8,  08);
    GET_REG(9,  09);
    GET_REG(10, 10);
    GET_REG(11, 11);
    GET_REG(12, 12);
    GET_REG(13, 13);
    GET_REG(14, 14);
    GET_REG(15, 15);
    GET_REG(16, 16);
    GET_REG(17, 17);
    GET_REG(18, 18);
    GET_REG(19, 19);
    GET_REG(20, 20);
    GET_REG(21, 21);
    GET_REG(22, 22);
    GET_REG(23, 23);
    GET_REG(24, 24);
    GET_REG(25, 25);

    // Explicitly use k0 ($26) to read fp regs 26-29 and gp regs
    // 28 & 29 as the GET_REG macro will try to manipulate sp & gp
    GET_FP_REG(26);
    GET_FP_REG(27);
    GET_FP_REG(28);
    GET_FP_REG(29);

    asm volatile( \
        "sd $28,%0 \n" \
        :"=m"(registers_after_ex[28])
    );

    asm volatile( \
        "sd $29,%0 \n" \
        :"=m"(registers_after_ex[29])
    );

    GET_REG(30, 30);
    GET_REG(31, 31);

    // Get lo & hi
    asm volatile(
        "mflo $26 \n"
        "sd $26,%0 \n"
        "mfhi $26 \n"
        "sd $26,%1 \n"
        :"=m"(lo), "=m"(hi)
        :
        :"$26"
    );

    ASSERT_EQUAL_SIGNED(exception_occurred, 1, "Exception was not triggered");

    ASSERT_REG(0,  00);
    ASSERT_REG(1,  01);
    ASSERT_REG(2,  02);
    ASSERT_REG(3,  03);
    ASSERT_REG(4,  04);
    ASSERT_REG(5,  05);
    ASSERT_REG(6,  06);
    ASSERT_REG(7,  07);
    ASSERT_REG(8,  08);
    ASSERT_REG(9,  09);
    ASSERT_REG(10, 10);
    ASSERT_REG(11, 11);
    ASSERT_REG(12, 12);
    ASSERT_REG(13, 13);
    ASSERT_REG(14, 14);
    ASSERT_REG(15, 15);

    ASSERT_REG(16, 16);
    ASSERT_REG(17, 17);
    ASSERT_REG(18, 18);
    ASSERT_REG(19, 19);
    ASSERT_REG(20, 20);
    ASSERT_REG(21, 21);
    ASSERT_REG(22, 22);
    ASSERT_REG(23, 23);
    ASSERT_REG(24, 24);
    ASSERT_REG(25, 25);

    ASSERT_EQUAL_HEX(registers_after_ex[28], gp, "$28 not saved");
    ASSERT_EQUAL_HEX(exception_regs.gpr[28], gp, "$28 not available to the handler");
    ASSERT_EQUAL_HEX(fp_registers_after_ex[28], 0x2828282828282828, "$f28 not saved");
    ASSERT_EQUAL_HEX(exception_regs.fpr[28], 0x2828282828282828, "$f28 not available to the handler");

    ASSERT_EQUAL_HEX(registers_after_ex[29], sp, "$29 not saved");
    ASSERT_EQUAL_HEX(exception_regs.gpr[29], sp, "$29 not available to the handler");
    ASSERT_EQUAL_HEX(fp_registers_after_ex[29], 0x2929292929292929, "$f29 not saved");
    ASSERT_EQUAL_HEX(exception_regs.fpr[29], 0x2929292929292929, "$f29 not available to the handler");

    ASSERT_REG(30, 30);
    ASSERT_REG(31, 31);

    ASSERT_EQUAL_HEX(lo, 0xDEADBEEFDEADBEEF, "lo not saved");
    ASSERT_EQUAL_HEX(exception_regs.lo, 0xDEADBEEFDEADBEEF, "lo not available to the handler");

    ASSERT_EQUAL_HEX(hi, 0xBEEFF00DBEEFF00D, "hi not saved");
    ASSERT_EQUAL_HEX(exception_regs.hi, 0xBEEFF00DBEEFF00D, "hi not available to the handler");

    // Other info
    ASSERT_EQUAL_HEX(exception_regs.epc, (uint32_t)&test_exception_opcode, "EPC not available to the handler");

    // If the other tests change SR these may fail unnecessarily, but we expect tests to do proper cleanup
    ASSERT_EQUAL_HEX(exception_regs.sr, 0x241004E3, "SR not available to the handler");
    ASSERT_EQUAL_HEX(exception_regs.cr, 0x8, "CR not available to the handler");
    ASSERT_EQUAL_HEX(exception_regs.fc31, 0x0, "FCR31 not available to the handler");
}

#undef SET_REG
#undef SET_FP_REG
#undef GET_FP_REG
#undef GET_REG
#undef ASSERT_REG_FP
#undef ASSERT_REG_GP
#undef ASSERT_REG_FP_HANDLER
#undef ASSERT_REG_GP_HANDLER
#undef ASSERT_REG


static volatile bool tsh_called = true;
static volatile uint32_t tsh_code = 0;

void test_syscall_handler(exception_t *exc, uint32_t code) {
    tsh_called = true;
    tsh_code = code;
}

void test_exception_syscall(TestContext *ctx) {
    static bool registered = false;
    if (!registered) {
        register_syscall_handler(test_syscall_handler, 0x0F100, 0x0F10F);
        registered = false;
    }

    tsh_called = false;
    asm volatile ("syscall 0x0F108");
    ASSERT_EQUAL_SIGNED(tsh_called, true, "Syscall handler not called");
    ASSERT_EQUAL_HEX(tsh_code, 0x0F108, "Syscall handler called with wrong code");
}
