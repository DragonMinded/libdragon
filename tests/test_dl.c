
static volatile int interrupt_raised;

void sp_interrupt_handler()
{
    interrupt_raised = 1;
}

void test_dl_simple(TestContext *ctx)
{
    interrupt_raised = 0;

    register_SP_handler(sp_interrupt_handler);
    set_SP_interrupt(1);

    dl_init();
    DEFER(dl_close());

    dl_interrupt();

    const unsigned long timeout_ms = 100;
    unsigned long time = get_ticks_ms();

    while (1) {
        // Wait until the interrupt was raised and the SP is in idle mode
        if (interrupt_raised && (*SP_STATUS & SP_STATUS_HALTED)) {
            break;
        }

        // Assert if the timeout was hit
        unsigned long elapsed = get_ticks_ms() - time;
        ASSERT(elapsed < timeout_ms, "DL not finished after %lu ms! SP_STATUS: %#010lx", elapsed, *SP_STATUS);
    }

    ASSERT(interrupt_raised, "Interrupt was not raised!");
    ASSERT_EQUAL_HEX(*SP_STATUS, SP_STATUS_HALTED | SP_STATUS_INTERRUPT_ON_BREAK, "Unexpected SP status!");
}