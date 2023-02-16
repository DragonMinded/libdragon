#if DEBUG

#include <string.h>
#include <stdarg.h>
#include "pputils.h"

#define ROUND_UP(n, d) ({ \
	typeof(n) _n = n; typeof(d) _d = d; \
	(((_n) + (_d) - 1) / (_d) * (_d)); \
})

volatile uint32_t *s8_register asm ("s8");

#define DATATYPE_TEXT       0x01
#define DEBUG_ADDRESS       (0xB3000000)

#define D64_CIBASE_ADDRESS 0xB8000000

#define D64_REGISTER_STATUS  0x00000200
#define D64_REGISTER_COMMAND 0x00000208
#define D64_REGISTER_LBA     0x00000210
#define D64_REGISTER_LENGTH  0x00000218
#define D64_REGISTER_RESULT  0x00000220

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

static void usb_64drive_wait(void)
{
    while ((io_read(D64_CIBASE_ADDRESS + D64_REGISTER_STATUS) >> 8) & D64_CI_BUSY) {}
}

static void usb_64drive_waitidle()
{
    while (((io_read(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT) >> 4) & D64_USB_BUSY) != D64_USB_IDLE) {
        uint32_t x = io_read(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT);
        io_write(DEBUG_ADDRESS, x);
    }
}

static void usb_64drive_setwritable(bool enable)
{
    usb_64drive_wait();
    io_write(D64_CIBASE_ADDRESS + D64_REGISTER_COMMAND, 
        enable ? D64_ENABLE_ROMWR : D64_DISABLE_ROMWR);
    usb_64drive_wait();
}

static void usb_flush(int size)
{
    io_write(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, D64_COMMAND_WRITE);
    io_write(D64_CIBASE_ADDRESS + D64_REGISTER_USBP0R0, (DEBUG_ADDRESS) >> 1);
    io_write(D64_CIBASE_ADDRESS + D64_REGISTER_USBP1R1, (size & 0xFFFFFF) | (DATATYPE_TEXT << 24));
}

__attribute__((noinline))
static void _usb_print(int ssize, const char *string, int nargs, ...)
{
    static uint32_t addr = DEBUG_ADDRESS;
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

    io_write(addr, 0x0A000000), addr += 4;

    #if 0
    // FIXME: this does work only once, then the USB interface stays busy forever?
    usb_flush(addr - DEBUG_ADDRESS);
    usb_64drive_waitidle();
    #endif
}

#define debugf(s, ...)   _usb_print(__builtin_strlen(s), s "    ", __COUNT_VARARGS(__VA_ARGS__), ##__VA_ARGS__)

static void usb_init(void)
{
    usb_64drive_setwritable(true);
    for (int i = 0; i < 0x1000; i += 4)
        io_write(DEBUG_ADDRESS + i, 0);
}

#else 

static void usb_init(void) {}
#define debugf(s, ...) ({ })

#endif

