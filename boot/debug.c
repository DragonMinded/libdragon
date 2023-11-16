/**
 * @file debug.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief IPL3 debugging module
 * 
 * This module implements a simple debugging interface for IPL3. The debugging
 * messages are forward to both ISViewer (for emulators), and USB for the
 * 64drive and the SummerCart64.
 * 
 * In production mode, the debug code is not linked at all. In general,
 * the ROM production layout does not even allow to link the debug code
 * as the space available (4 KiB) would not be enough.
 * 
 * In development mode, the debug code is linked but it is not loaded into DMEM.
 * In fact, the linker script setups relocations so that IPL3 calls debugging
 * code directly into ROM (by jumping into the cartridge space at 0xB0000000).
 * Running code directly from ROM is in fact possible, though it is very slow,
 * but this is not a big problem. This allows us to link "as much" debugging
 * support code as we want, without worrying about the 4 KiB limit.
 * 
 * When running code from ROM, the CPU fetches each opcode via PI. This means
 * that it is not possible to *write* to PI while running code from ROM,
 * because subsequent reads are not correctly synchronized (given that PI
 * writes are asynchronous). Writing to flashcart registers requires writing
 * to the PI space; to workaround this, the debugging code calls a io_write
 * function (defined in minidragon.c) which is instead loaded in DMEM as part
 * of the IPL3 code. This is the only debugging-related function stored in
 * DMEM with the rest of IPL3, but it's very little (only a handful of
 * instructions).
 */

#include "debug.h"

#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include "minidragon.h"

#define ROUND_UP(n, d) ({ \
	typeof(n) _n = n; typeof(d) _d = d; \
	(((_n) + (_d) - 1) / (_d) * (_d)); \
})

#define DATATYPE_TEXT       0x01
#define D64_DEBUG_ADDRESS   (0xB3000000)

#define D64_CIBASE_ADDRESS 0xB8000000

#define D64_REGISTER_STATUS  0x00000200
#define D64_REGISTER_COMMAND 0x00000208
#define D64_REGISTER_LBA     0x00000210
#define D64_REGISTER_LENGTH  0x00000218
#define D64_REGISTER_RESULT  0x00000220
#define D64_REGISTER_MAGIC   0x000002EC

#define D64_REGISTER_USBCOMSTAT 0x00000400
#define D64_REGISTER_USBP0R0    0x00000404
#define D64_REGISTER_USBP1R1    0x00000408

#define D64_ENABLE_ROMWR  0xF0
#define D64_DISABLE_ROMWR 0xF1
#define D64_COMMAND_WRITE 0x08

#define D64_USB_IDLE        0x00
#define D64_USB_IDLEUNARMED 0x00
#define D64_USB_ARMED       0x01
#define D64_USB_DATA        0x02
#define D64_USB_ARM         0x0A
#define D64_USB_BUSY        0x0F
#define D64_USB_DISARM      0x0F
#define D64_USB_ARMING      0x0F

#define D64_CI_IDLE  0x00
#define D64_CI_BUSY  0x10
#define D64_CI_WRITE 0x20

#define D64_MAGIC 0x55444236

#define IQUE_DEBUG_ADDRESS  (0x807C0000)

#define ISVIEWER_WRITE_LEN       (0xB3FF0014)
#define ISVIEWER_BUFFER          (0xB3FF0020)
#define ISVIEWER_BUFFER_LEN      0x00000200


#define DEBUG_PIPE_ISVIEWER        1
#define DEBUG_PIPE_64DRIVE         2
#define DEBUG_PIPE_SC64            3   // to be implemented
#define DEBUG_PIPE_IQUE            4

static uint32_t debug_pipe = 0;
static uint32_t ique_addr = IQUE_DEBUG_ADDRESS;


// Call io_write (defined in minidragon.c) which is in DMEM with the rest of IPL3.
// Since debugging code is instead run directly from ROM, we need a far call.
__attribute__((far))
extern void io_write(uint32_t vaddrx, uint32_t value);

static void usb_64drive_wait(void)
{
    while ((io_read(D64_CIBASE_ADDRESS + D64_REGISTER_STATUS) >> 8) & D64_CI_BUSY) {}
}

static void usb_64drive_waitidle()
{
    while (((io_read(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT) >> 4) & D64_USB_BUSY) != D64_USB_IDLE) {}
}

static void usb_64drive_setwritable(bool enable)
{
    usb_64drive_wait();
    io_write(D64_CIBASE_ADDRESS + D64_REGISTER_COMMAND, 
        enable ? D64_ENABLE_ROMWR : D64_DISABLE_ROMWR);
    usb_64drive_wait();
}

static uint32_t usb_print_begin(void)
{
    switch (debug_pipe) {
    case DEBUG_PIPE_ISVIEWER:
        return ISVIEWER_BUFFER;
    case DEBUG_PIPE_64DRIVE:
        return D64_DEBUG_ADDRESS;
    case DEBUG_PIPE_IQUE:
        return ique_addr;
    default:
        return 0;
    }
}

static void usb_print_end(int nbytes)
{
    switch (debug_pipe) {
    case DEBUG_PIPE_ISVIEWER:
        io_write(ISVIEWER_WRITE_LEN, nbytes);
        return;
    case DEBUG_PIPE_64DRIVE:
        io_write(D64_CIBASE_ADDRESS + D64_REGISTER_USBP0R0, (D64_DEBUG_ADDRESS) >> 1);
        io_write(D64_CIBASE_ADDRESS + D64_REGISTER_USBP1R1, (nbytes & 0xFFFFFF) | (DATATYPE_TEXT << 24));
        io_write(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, D64_COMMAND_WRITE);
        usb_64drive_waitidle();
        return;
    case DEBUG_PIPE_IQUE:
        ique_addr += nbytes;
        return;
    }
    __builtin_unreachable();
}


void _usb_print(int ssize, const char *string, int nargs, ...)
{
    uint32_t addr_start = usb_print_begin();
    if (!addr_start) return;
    uint32_t addr = addr_start;

    uint32_t *s = (uint32_t*)string;
    for (; ssize > 0; ssize -= 4, addr += 4)
        io_write(addr, *s++);

    if (nargs) {
        va_list ap;
        va_start(ap, nargs);
        for (int i = 0; i < nargs; i++) {
            uint32_t x = va_arg(ap, uint32_t);
            uint64_t hex = 0;
            for (int j = 0; j < 8; j++) {
                uint32_t c = (x >> 28) & 0xF;
                if (c < 10) c += '0';
                else c += 'A' - 10;
                hex = (hex << 8) | c;
                x <<= 4;
            }
            io_write(addr, hex >> 32), addr += 4;
            io_write(addr, hex >>  0), addr += 4;
            if (i < nargs - 1)
                io_write(addr, 0x20202020), addr += 4;
        }
    }

    io_write(addr, 0x2020200A), addr += 4;
    usb_print_end(addr - addr_start);
}

static int usb_detect(void)
{
	io_write(ISVIEWER_BUFFER, 0x12345678);
	if (io_read(ISVIEWER_BUFFER) == 0x12345678)
        return DEBUG_PIPE_ISVIEWER;

    if (io_read(D64_CIBASE_ADDRESS + D64_REGISTER_MAGIC) == D64_MAGIC)
        return DEBUG_PIPE_64DRIVE;

    if ((*MI_VERSION & 0xF0) == 0xB0)
        return DEBUG_PIPE_IQUE;
    
    return 0;
}


void usb_init(void)
{
    debug_pipe = usb_detect();

    // Pipe-specific initializations
    switch (debug_pipe) {
    case DEBUG_PIPE_64DRIVE:
        usb_64drive_setwritable(true);
        for (int i = 0; i < 0x1000; i += 4)
            io_write(D64_DEBUG_ADDRESS + i, 0);
        break;
    }
}
