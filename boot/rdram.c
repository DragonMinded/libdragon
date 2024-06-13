/**
 * @file ipl3.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief RDRAM initialization module
 * 
 * This module contains the RDRAM initialization process. The process is quite
 * convoluted and is based mainly on the Rambus RDRAM datasheets and the
 * information found on the n64brew wiki, plus direct experimentation. 
 * 
 * Please follows the comments in rdram_init() to have an overview of the
 * process.
 */
#include "minidragon.h"
#include "debug.h"
#include "entropy.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** 
 * Change to 1 to activate support for obsolete HW1 RCP.
 * This version has a different layout for the RDRAM registers.
 * No known commercial unit is using this, so it is disabled by
 * default.
 */
#define SUPPORT_HW1   0

// Memory map exposed by RI to the CPU
#define RDRAM                               ((volatile void*)0xA0000000)
#define RDRAM_REGS                          ((volatile uint32_t*)0xA3F00000)
#define RDRAM_REGS_BROADCAST                ((volatile uint32_t*)0xA3F80000)

#define RI_CONFIG_AUTO_CALIBRATION			0x40
#define RI_SELECT_RX_TX						0x14
#define RI_MODE_CLOCK_TX                    0x8
#define RI_MODE_CLOCK_RX                    0x4
#define RI_MODE_RESET						0x0
#define RI_MODE_STANDARD					(0x2 | RI_MODE_CLOCK_RX | RI_MODE_CLOCK_TX)
#define RI_REFRESH_CLEANDELAY(x)            ((x) & 0xFF)
#define RI_REFRESH_DIRTYDELAY(x)            (((x) & 0xFF) << 8)
#define RI_REFRESH_AUTO                     (1<<17)
#define RI_REFRESH_OPTIMIZE                 (1<<18)
#define RI_REFRESH_MULTIBANK(x)             (((x) & 0xF) << 19)

// Maximum number of DeviceID value that we can configure a chip with.
// IDs >= 512 are technically valid, but registers can't be accessed anymore.
#define RDRAM_MAX_DEVICE_ID                 511

#define RDRAM_BROADCAST                     -1
typedef enum {
    RDRAM_REG_DEVICE_TYPE = 0,              // Read-only register which describes RDRAM configuration
    RDRAM_REG_DEVICE_ID = 1,                // Specifies base address of RDRAM
    RDRAM_REG_DELAY = 2,                    // Specifies CAS timing parameters
    RDRAM_REG_MODE = 3,                     // Control operating mode and IOL output current
    RDRAM_REG_REF_INTERVAL = 4,             // Specifies refresh interval for devices that require refresh
    RDRAM_REG_REF_ROW = 5,                  // Next row and bank to be refreshed
    RDRAM_REG_RAS_INTERVAL = 6,             // Specifies AS access interval
    RDRAM_REG_MIN_INTERVAL = 7,             // Provides minimum delay information and some special control
    RDRAM_REG_ADDR_SELECT = 8,              // Specifies Adr field subfield swapping to maximize hit rate
    RDRAM_REG_DEVICE_MANUFACTURER = 9,      // Read-only register providing manufacturer and device information

    // This can only be accessed on RAC v2 (as registers are more spaced)
    RDRAM_REG_ROW = 128,                    // Address of currently sensed row in each bank
} rdram_reg_t;

#define BIT(x, n)           (((x) >> (n)) & 1)
#define BITS(x, b, e)       (((x) >> (b)) & ((1 << ((e)-(b)+1))-1))
#define BITSWAP5(x)         ((BIT(x,0)<<4) | (BIT(x,1)<<3) | (BIT(x,2)<<2) | (BIT(x,3)<<1) | (BIT(x,4)<<0))

// Create a RDRAM_REG_DELAY value from the individual timings
#define RDRAM_REG_DELAY_MAKE(write, ack, read, ackwin) \
    (((write) & 0xF) << (24+3)) | (((ack) & 0xF) << (16+3)) | (((read) & 0xF) << (8+3)) | (((ackwin) & 0xF) << (0+3))

// Create a RDRAM_REG_RASINTERVAL value from individual timings
#define RDRAM_REG_RASINTERVAL_MAKE(row_precharge, row_sense, row_imp_restore, row_exp_restore) \
    (BITSWAP5(row_precharge) | (BITSWAP5(row_sense) << 8) | (BITSWAP5(row_imp_restore) << 16) | (BITSWAP5(row_exp_restore) << 24))
    
enum {
    MANUFACTURER_TOSHIBA = 0x2,
    MANUFACTURER_FUJITSU = 0x3,
    MANUFACTURER_NEC = 0x5,
    MANUFACTURER_HITACHI = 0x7,
    MANUFACTURER_OKI = 0x9,
    MANUFACTURER_LG = 0xA,
    MANUFACTURER_SAMSUNG = 0x10,
    MANUFACTURER_HYNDAI = 0x13,
} RDRAM_MANUFACTURERS;

typedef struct {
    int manu;              // Manufacturer company
    int code;              // Internal product ID (assigned by the company)
} rdram_reg_manufacturer_t;

typedef struct {
    int version;
    int type;
    int row_bits;
    int bank_bits;
    int col_bits;
    int ninth_bit;
    int low_latency;
} rdram_reg_devicetype_t;

static uint32_t mi_version_io(void)  { return (*MI_VERSION >>  0) & 0xFF; }

register uint32_t rdram_reg_stride_shift asm("k0");

static inline void rdram_reg_w(int chip_id, rdram_reg_t reg, uint32_t value)
{
    // RDRAM registers are physically little-endian. Swap them on write, so that
    // the rest of the code follows the datasheets.
    value = byteswap32(value);
    if (chip_id < 0) {
        RDRAM_REGS_BROADCAST[reg] = value;
        // debugf("rdram_reg_w: ", (uint32_t)(&RDRAM_REGS_BROADCAST[reg]), value);
    } else {
        assert(chip_id < 512);
        RDRAM_REGS[(chip_id << rdram_reg_stride_shift) + reg] = value;
        // debugf("rdram_reg_w: ", (uint32_t)(&RDRAM_REGS[(chip_id << rdram_reg_stride_shift) + reg]), value);
    }
}

static inline uint32_t rdram_reg_r(int chip_id, rdram_reg_t reg)
{
    MEMORY_BARRIER();
    if (reg & 1) *MI_MODE = MI_WMODE_SET_UPPER_MODE;
    uint32_t value = RDRAM_REGS[(chip_id << rdram_reg_stride_shift) + reg];
    if (reg & 1) *MI_MODE = MI_WMODE_CLEAR_UPPER_MODE;
    MEMORY_BARRIER();
    return byteswap32(value);
}

static void rdram_reg_init(void) {
    // On RI v1, registers are 0x200 bytes apart
    uint32_t version = SUPPORT_HW1 ? mi_version_io() : 2;
    debugf("rdram_reg_init: IO version ", version);
    switch (version) {
    case 1:  rdram_reg_stride_shift =  9-2; break;
    default: rdram_reg_stride_shift = 10-2; break;
    }

    // We must initialize the Delay timing register before accessing any other register.
    // This is tricky before RI hardcodes the write delay to 1 cycle, but the RDRAM
    // chips default to 4, so writing the Delay register is a chicken-egg problem.
    // The solution here is to use the special "MI repeat mode" where the same
    // written value is repeated N times. On top of that, we need to also half-rotate
    // the value because the initial Delay of 4 put it out-of-phase
    uint32_t delay = RDRAM_REG_DELAY_MAKE(1, 3, 7, 5);

    // Rotate the value to allow for out-of-phase write
    delay = (delay >> 16) | (delay << 16);

    // Activate MI repeat mode and write the value to all chips (via broadcast)
    *MI_MODE = MI_WMODE_SET_REPEAT_MODE | MI_WMODE_REPEAT_LENGTH(16);
    rdram_reg_w(RDRAM_BROADCAST, RDRAM_REG_DELAY, delay);
}


static void rdram_reg_w_deviceid(int chip_id, uint16_t new_chip_id)
{   
    uint32_t value = 0;

    value |= ((new_chip_id >>  0) & 0x03F) <<  2;   // Bits 0..5
    value |= ((new_chip_id >>  6) & 0x1FF) << 15;   // Bits 6..14
    value |= ((new_chip_id >> 15) & 0x001) << 31;   // Bit  15

    rdram_reg_w(chip_id, RDRAM_REG_DEVICE_ID, value);
}

static void rdram_reg_r_mode(int nchip, int *cci)
{
    uint32_t value = rdram_reg_r(nchip, RDRAM_REG_MODE);

    if (cci) {
        uint8_t cc0 = (value >> 30) &  1;
        uint8_t cc1 = (value >> 21) &  2;
        uint8_t cc2 = (value >> 12) &  4;
        uint8_t cc3 = (value >> 28) &  8;
        uint8_t cc4 = (value >> 19) & 16;
        uint8_t cc5 = (value >> 10) & 32;
        *cci = (cc0 | cc1 | cc2 | cc3 | cc4 | cc5);
    }
}

/** 
 * Write the RDRAM mode register. This is mainly used to write the current (I) value
 * in manual/auto mode, so this helper function just allows to do that, with some
 * other fixed values.
 * 
 * @param nchip         Number of RDRAM chip to configure
 * @param auto_current  If true, set auto current mode, otherwise set manual mode
 * @param cci           Inverted current (I) value (0-63). "Inverted" means that the
 *                      range 0-63 is reversed wrt the register value, as this is
 *                      more intuitive (maps linearly to the current in mA).
 */

#define CCVALUE(cc) ((BIT(cc,0)<<30) | (BIT(cc,1)<<22) | (BIT(cc,2)<<14) | (BIT(cc,3)<<31) | (BIT(cc,4)<<23) | (BIT(cc,5)<<15))

static int rdram_reg_w_mode(int nchip, bool auto_current, uint8_t cci)
{
    uint8_t cc = cci ^ 0x3F;   // invert bits to non inverted value
    enum { 
        X2 = 1 << 6, // ?
        CURRENT_CONTROL_AUTO = 1 << 7, // Set auto current mode
        AUTO_SKIP = 1 << 2, // ?
        DEVICE_EN = 1 << 1, // Enable direct chip configuration (even without broadcast)
    };

    uint32_t value = DEVICE_EN | AUTO_SKIP | X2;
    if (auto_current) value |= CURRENT_CONTROL_AUTO;
    value |= CCVALUE(cc);

    rdram_reg_w(nchip, RDRAM_REG_MODE, value);

    if (auto_current) {
        // After setting auto mode, it is necessary to wait a little bit and then
        // poll the mode register two times to stabilize and readback the actual
        // current value. It seems that it's necessary to do this or some (RI's?)
        // internal state machine is stalled. This is still quite mysterious
        // as the CURRENT_CONTROL_AUTO (1<<7) is not part of any Rambus datasheet...
        int cc_read;
        wait(0x100);
        rdram_reg_r_mode(nchip, NULL);
        rdram_reg_r_mode(nchip, &cc_read);
        return cc_read;
    }

    return cc;
}

static rdram_reg_manufacturer_t rdram_reg_r_manufacturer(int nchip)
{
    uint32_t value = rdram_reg_r(nchip, RDRAM_REG_DEVICE_MANUFACTURER);
    return (rdram_reg_manufacturer_t){
        .manu = BITS(value, 16, 31),
        .code = BITS(value,  0, 15),
    };
}

static rdram_reg_devicetype_t rdram_reg_r_devicetype(int nchip)
{
    uint32_t value = rdram_reg_r(nchip, RDRAM_REG_DEVICE_TYPE);
    return (rdram_reg_devicetype_t){
        .version = BITS(value, 28, 31),
        .type = BITS(value, 24, 27),
        .row_bits = BITS(value, 8, 11),
        .bank_bits = BITS(value, 12, 15),
        .col_bits = BITS(value, 4, 7),
        .ninth_bit = BIT(value, 2),
        .low_latency = BIT(value, 0),
    };
}

static float memory_test(volatile uint32_t *vaddr) {
    enum { NUM_TESTS = 10 };

    float accuracy = 0.0f;
    for (int t=0; t<NUM_TESTS; t++) {
        // Write test words
        vaddr[0] = 0xFFFFFFFF;
        vaddr[1] = 0xFFFFFFFF;

        // Read back one byte and count number of set bits
        volatile uint8_t b0 = ((volatile uint8_t*)vaddr)[5];
        while (b0) {
            if (b0 & 1) accuracy += 1.0f;
            b0 >>= 1;
        }
    }

    // In case of non-zero accuracy, some bits were decayed and others might not.
    // This random pattern is a good source of entropy.
    if (accuracy > 0)
        entropy_add(vaddr[1]);

    return accuracy * (1.0f / (NUM_TESTS * 8));
}

static int rdram_calibrate_current(uint16_t chip_id)
{
    float weighted_sum = 0.0f;
    float prev_accuracy = 0.0f;
    volatile uint32_t *vaddr = RDRAM + chip_id * 1024 * 1024;

    for (int cc=0; cc<64; cc++) {
        // Go through all possible current values, in ascending order, in "manual" mode.
        rdram_reg_w_mode(chip_id, false, cc);

        // Perform a memory test to check how stable the RDRAM is at this current level.
        // Calculate a weighted sum between the different current levels.
        float accuracy = memory_test(vaddr);
        // debugf("rdram_calibrate_current: accuracy ", cc, (uint32_t)(accuracy * 255));
        weighted_sum += (accuracy - prev_accuracy) * cc;

        // Stop once we reach full accuracy.
        if (accuracy >= 1.0f) break;
        prev_accuracy = accuracy;
    }

    // debugf("rdram_calibrate_current: weighted sum ", (int)weighted_sum);
    int target_cc = weighted_sum * 2.2f + 0.5f;
    if (!target_cc)
        return 0;
    // debugf("rdram_calibrate_current: target_cc ", target_cc);

    // Now we want to configure the automatic mode. Unfortunately, the cc values
    // written in automatic mode have a different scale compared to manual mode.
    int minerr = 0;
    int autocc = 0;
    for (int cc=0; cc<64; cc++) {
        // Write the CC value in automatic mode.
        int cc_readback = rdram_reg_w_mode(chip_id, true, cc);

        // debugf("rdram_calibrate_current: auto ", cc, cc_readback);
        int err = abs(cc_readback - target_cc);
        if (cc == 0 || err < minerr) {
            minerr = err;
            autocc = cc;
        }
        if (cc_readback > target_cc)
            break;
    }

    // debugf("rdram_calibrate_current: auto_cc ", autocc, minerr);
    return autocc;
}

int rdram_init(void (*bank_found)(int chip_id, bool last))
{
    // Start current calibration. This is necessary to ensure the RAC outputs
    // the correct current value to talk to RDRAM chips.
    *RI_CONFIG = RI_CONFIG_AUTO_CALIBRATION;   // Turn on the RI auto current calibration
    wait(0x100);                               // Wait for calibration
    *RI_CURRENT_LOAD = 0;                      // Apply the calibrated value
    
    // Activate communication with RDRAM chips. We can't do this before current calibration
    // as the chips might not be able to communicate correctly if the current is wrong.
    *RI_SELECT = RI_SELECT_RX_TX;

    // Reset chips. After the reset, all chips are turned off (DE=0 in MODE), and
    // mapped to device ID 0.
    *RI_MODE = RI_MODE_RESET;    wait(0x100);
    *RI_MODE = RI_MODE_STANDARD; wait(0x100);

    // Initialize RDRAM register access
    rdram_reg_init();

    // First call to callback, now that RI is initialized
    bank_found(-1, false);

    // Follow the init procedure specified in the datasheet.
    // First, put all of them to a fixed high ID (we use RDRAM_MAX_DEVICE_ID).
    enum { 
        INITIAL_ID = RDRAM_MAX_DEVICE_ID,
        INVALID_ID = RDRAM_MAX_DEVICE_ID - 2,
    };
    rdram_reg_w_deviceid(RDRAM_BROADCAST, INITIAL_ID);
    rdram_reg_w(RDRAM_BROADCAST, RDRAM_REG_MODE, (1<<6)|(1<<2));
    rdram_reg_w(RDRAM_BROADCAST, RDRAM_REG_REF_ROW, 0);

    // Initialization loop
    int total_memory = 0;
    int chip_id = 0; 
    while (1) {
        // Change the device ID to chip_id, which is our progressive ID. All chips
        // now are mapped to INITIAL_ID, but only the first one in the chain will
        // actually catch this non-broadcast command and change its ID. We use
        // this trick to configure one chip at a time.
        rdram_reg_w_deviceid(INITIAL_ID, chip_id);

        // Turn on the chip (set DE=1)
        rdram_reg_w(chip_id, RDRAM_REG_MODE, (1<<6) | (1<<1) | (1<<2));

        // Check if the DE bit was turned on. If it's not, a chip is not present
        // and we can abort the initialization loop.
        if (!(rdram_reg_r(chip_id, RDRAM_REG_MODE) & (1<<1))) {
            if (chip_id)
                bank_found(chip_id-2, true);
            break;
        }

        // Call the callback on the previous chip (if this is not the first),
        // now that we know if it's the last one or not.
        if (chip_id)
            bank_found(chip_id-2, false);

        // Calibrate the chip current. n64brew suggests to do 4 attempts here
        // but our tests seem to indicate that results are really stable and
        // a single attempt seems enough.
        enum { NUM_CALIBRATION_ATTEMPTS = 1 };
        int weighted_cc = 0;
        for (int i=0; i<NUM_CALIBRATION_ATTEMPTS; i++) {
            int cc = rdram_calibrate_current(chip_id);
            if (!cc) break;
            weighted_cc += cc;
        }
        if (!weighted_cc) {
            debugf("error: current calibration failed for chip_id ", chip_id);
            rdram_reg_w_deviceid(chip_id, INVALID_ID);
            break;
        }
        int target_cc = weighted_cc / NUM_CALIBRATION_ATTEMPTS;
        rdram_reg_w_mode(chip_id, true, target_cc);
   
        // Now that we have calibrated the output current of the chip, we are able
        // to actually read data from it. Read the manufacturer code and the device type.
        rdram_reg_devicetype_t t = rdram_reg_r_devicetype(chip_id);

        // Check that the chip matches the expected geometry. Notice that we only
        // support 2 MiB chips at this point, as there are no known commercial units
        // with 1 MiB chips (also Nintendo IPL3 is broken for 1 MiB chips anyway).
        // Notice also that "4 MiB chips" are actually two different 2 MiB chips
        // in the same package, so we actually see them as two different chips.
        if (t.bank_bits != 1 || t.row_bits != 9 || t.col_bits != 0xB || t.ninth_bit != 1) {
            debugf("error: invalid geometry: ", t.version, t.type, t.row_bits, t.bank_bits, t.col_bits, t.ninth_bit, t.low_latency);
            rdram_reg_w_deviceid(chip_id, INVALID_ID);
            break;
        }

        // Read the manufacturer to configure the timing
        rdram_reg_manufacturer_t m = rdram_reg_r_manufacturer(chip_id);

        uint32_t ras_interval;
        switch (m.manu) {
            case MANUFACTURER_NEC:
                // From NEC datasheet
                ras_interval = RDRAM_REG_RASINTERVAL_MAKE(1, 7, 10, 4);
                break;
            default:
                // For anything else, use the low latency bit to try to configure
                // the correct RAS intervals
                if (t.low_latency)
                    ras_interval = RDRAM_REG_RASINTERVAL_MAKE(1, 7, 10, 4);
                else
                    ras_interval = RDRAM_REG_RASINTERVAL_MAKE(2, 6,  9, 4);            
                break;
        }
        rdram_reg_w(chip_id, RDRAM_REG_RAS_INTERVAL, ras_interval);

        // Touch each bank of RDRAM to "settle timing circuits". We need to touch
        // memory every 512 KiB, so it's 4 iterations for 2 MiB chips.
        for (int bank=0; bank<4; bank++) {
            volatile uint32_t *ptr = RDRAM + chip_id * 1024 * 1024 + bank * 512 * 1024;
            // ptr[2]=0; ptr[3]=0;
            (void)ptr[0]; (void)ptr[1];
        }

        debugf("Chip: ", chip_id);
        debugf("\tManufacturer: ", m.manu);
        debugf("\tGeometry: ", t.bank_bits, t.row_bits, t.col_bits);
        debugf("\tCurrent: ", target_cc);
        debugf("\tRAS: ", ras_interval);

        // The chip has been configured and mapped. Now go to the next chip.
        // 2 MiB chips must be mapped at even addresses (as they cover two
        // 1-MiB areas), so increase chip-id by 2
        chip_id += 2;
        total_memory += 2*1024*1024;
    }

    // Setup now the RI refresh register, so that RDRAM chips get refreshed
    // at each HSYNC.
    // This register contains a bitmask that indicates which RDRAM chips are
    // multibank (aka 2 MiB). The bitmask has only 4 bits, so we can only
    // configure 4 chips (which is enough for 4x2MiB = 8MiB total RDRAM).
    // Since we only support 2 MiB chips anyway, we fill the bitmask with 1s
    // corresponding to the number of chips that were found.
    int refresh_multibanks = (1 << (chip_id >> 1)) - 1;
    *RI_REFRESH = RI_REFRESH_AUTO | RI_REFRESH_OPTIMIZE | RI_REFRESH_CLEANDELAY(52) | RI_REFRESH_DIRTYDELAY(54) | RI_REFRESH_MULTIBANK(refresh_multibanks);
    (void)*RI_REFRESH; // A dummy read seems necessary

    return total_memory;
}
