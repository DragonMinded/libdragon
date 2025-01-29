#include <stdint.h>
#include <stdbool.h>
#include "dd.h"
#include "interrupt.h"
#include "dma.h"

#define DD_ASIC_STATUS_MECHA_IRQ_LINE    (1<<9)
#define DD_ASIC_STATUS_BM_IRQ_LINE       (1<<10)

#define DD_ASIC_WCTRL_MECHA_IRQ_CLEAR    (1<<8)

bool dd_found = false;

static volatile int mecha_irq_count = 0;
static volatile int bm_irq_count = 0;

void dd_write(uint32_t address, uint16_t value) {
	io_write(address, value << 16);
}

uint16_t dd_read(uint32_t address) {
	return io_read(address) >> 16;
}

static void dd_handler(void)
{
    uint16_t status = dd_read(DD_ASIC_STATUS);

    if (status & DD_ASIC_STATUS_MECHA_IRQ_LINE) {
        // Acknowledge interrupt and record it was generated
        mecha_irq_count++;
        dd_write(DD_ASIC_WCTRL, DD_ASIC_WCTRL_MECHA_IRQ_CLEAR);
    }

    if (status & DD_ASIC_STATUS_BM_IRQ_LINE) {
        // This interrupt is auto-acknowledged when we read the ASIC status,
        // so just record it was generated
        bm_irq_count++;
    }
}

uint16_t dd_command(dd_cmd_t cmd) {
    int irq_count = mecha_irq_count;
	dd_write(DD_ASIC_WCMD, cmd);

    while (mecha_irq_count == irq_count) {}
	return dd_read(DD_ASIC_DATA);
}

__attribute__((constructor))
void dd_init(void) 
{
	uint32_t magic = 0x36344444; // "64DD"
	dd_found = io_read(0x06000020) == magic;
    if (!dd_found) return;

    // Install the cart interrupt handler immediately, because the DD will generate
    // interrupts as soon as you interact with it, and we need to acknowledge
    // them to avoid stalling the CPU.
    set_CART_interrupt(1);
    register_CART_handler(dd_handler);
}
