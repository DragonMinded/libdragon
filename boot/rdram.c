/** 
 * Change to 1 to activate support for obsolete HW1 RCP.
 * This version has a different layout for the RDRAM registers.
 * No known commercial unit is using this, so it is disabled by
 * default.
 */
#define SUPPORT_HW1   0

#define MI_MODE                             ((volatile uint32_t*)0xA4300000)
#define MI_WMODE_CLEAR_REPEAT_MOD           0x80
#define MI_WMODE_SET_REPEAT_MODE            0x100
#define MI_WMODE_REPEAT_LENGTH(n)           ((n)-1)
#define MI_WMODE_SET_UPPER_MODE             0x2000
#define MI_WMODE_CLEAR_UPPER_MODE           0x1000
#define MI_VERSION                          ((volatile uint32_t*)0xA4300004)

#define RI_MODE								((volatile uint32_t*)0xA4700000)
#define RI_CONFIG							((volatile uint32_t*)0xA4700004)
#define RI_CURRENT_LOAD						((volatile uint32_t*)0xA4700008)
#define RI_SELECT							((volatile uint32_t*)0xA470000C)
#define RI_REFRESH							((volatile uint32_t*)0xA4700010)
#define RI_LATENCY							((volatile uint32_t*)0xA4700014)
#define RI_RERROR							((volatile uint32_t*)0xA4700018)
#define RI_WERROR							((volatile uint32_t*)0xA470001C)

// Memory map exposed by RI to the CPU
#define RDRAM                               ((void*)0xA0000000)
#define RDRAM_REGS                          ((volatile uint32_t*)0xA3F00000)
#define RDRAM_REGS_BROADCAST                ((volatile uint32_t*)0xA3F80000)

#define RI_CONFIG_AUTO_CALIBRATION			0x40
#define RI_SELECT_RX_TX						0x14
#define RI_MODE_CLOCK_TX                    0x8
#define RI_MODE_CLOCK_RX                    0x4
#define RI_MODE_RESET						0x0
#define RI_MODE_STANDARD					(0x2 | RI_MODE_CLOCK_RX | RI_MODE_CLOCK_TX)

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

// Create a RDRAM_REG_DELAY value from the individual timings
#define RDRAM_REG_DELAY_MAKE(write, ack, read, ackwin) \
    (((write) & 0xF) << (24+3)) | (((ack) & 0xF) << (16+3)) | (((read) & 0xF) << (8+3)) | (((ackwin) & 0xF) << (0+3))

static void rdram_dump_regs(void)
{
    debugf("\tMI_MODE: ", *MI_MODE);
    debugf("\tMI_VERSION: ", *MI_VERSION);
    debugf("\tRI_MODE: ", *RI_MODE);
    debugf("\tRI_CONFIG: ", *RI_CONFIG);
    debugf("\tRI_CURRENT_LOAD: ", *RI_CURRENT_LOAD);
    debugf("\tRI_SELECT: ", *RI_SELECT);
    debugf("\tRI_LATENCY: ", *RI_LATENCY);
}

static uint32_t mi_version_rsp(void) { return (*MI_VERSION >> 24) & 0xFF; }
static uint32_t mi_version_rdp(void) { return (*MI_VERSION >> 16) & 0xFF; }
static uint32_t mi_version_rac(void) { return (*MI_VERSION >>  8) & 0xFF; }
static uint32_t mi_version_io(void)  { return (*MI_VERSION >>  0) & 0xFF; }

register uint32_t rdram_reg_stride_shift asm("k0");

static void rdram_reg_w(int chip_id, rdram_reg_t reg, uint32_t value)
{
    // RDRAM registers are physically little-endian. Swap them on write, so that
    // the rest of the code follows the datasheets.
    value = byteswap32(value);
    if (chip_id < 0) {
        RDRAM_REGS_BROADCAST[reg] = value;
        debugf("rdram_reg_w: ", (uint32_t)(&RDRAM_REGS_BROADCAST[reg]), value);
    } else {
        assert(chip_id < 512);
        RDRAM_REGS[(chip_id << rdram_reg_stride_shift) + reg] = value;
        debugf("rdram_reg_w: ", (uint32_t)(&RDRAM_REGS[(chip_id << rdram_reg_stride_shift) + reg]), value);
    }
}

static uint32_t rdram_reg_r(int chip_id, rdram_reg_t reg)
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
static void rdram_reg_w_mode(int nchip, bool auto_current, uint8_t cci)
{
    uint8_t cc = cci ^ 0x3F;   // invert bits to non inverted value
    enum { 
        CURRENT_CONTROL_AUTO = 1 << 7, // Set auto current mode
        CURRENT_CONTROL_MULT = 1 << 6, // ?
        AUTO_SKIP = 1 << 2, // ?
        DEVICE_EN = 1 << 1, // Enable direct chip configuration (even without broadcast)
    };

    uint32_t value = DEVICE_EN | AUTO_SKIP | CURRENT_CONTROL_MULT;
    if (auto_current) value |= CURRENT_CONTROL_AUTO;
    value |= ((cc >> 0) & 1) << 30;
    value |= ((cc >> 1) & 1) << 22;
    value |= ((cc >> 2) & 1) << 14;
    value |= ((cc >> 3) & 1) << 31;
    value |= ((cc >> 4) & 1) << 23;
    value |= ((cc >> 5) & 1) << 15;

    rdram_reg_w(nchip, RDRAM_REG_MODE, value);
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

// static void rdram_reg_w_delay(int chip_id, uint32_t value)
// {
//     rdram_reg_w(chip_id, RDRAM_REG_DELAY, value);
// }

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
    return accuracy * (1.0f / (NUM_TESTS * 8));
}

void rdram_calibrate_current(uint16_t chip_id)
{
    float weighted_sum = 0.0f;
    float prev_accuracy = 0.0f;
    uint32_t *vaddr = RDRAM + chip_id * 1024 * 1024;

    for (int cc=0; cc<64; cc++) {
        // Go through all possible current values, in ascending order, in "manual" mode.
        rdram_reg_w_mode(chip_id, false, cc);

        // Perform a memory test to check how stable the RDRAM is at this current level.
        // Calculate a weighted sum between the different current levels.
        float accuracy = memory_test(vaddr);
        debugf("rdram_calibrate_current: accuracy ", cc, (uint32_t)(accuracy * 255));
        weighted_sum += (accuracy - prev_accuracy) * cc;

        // Stop once we reach full accuracy.
        if (accuracy >= 1.0f) break;
    }

    int target_cc = weighted_sum * 2.2f + 0.5f; (void)target_cc;
    debugf("rdram_calibrate_current: target_cc ", target_cc);

    // Now we want to configure the automatic mode. Unfortunately, the cc values
    // written in automatic mode have a different scale compared to manual mode.
    float minerr = 0;
    int mincc = 0;
    for (int cc=0; cc<64; cc++) {
        // Write the CC value in automatic mode.
        rdram_reg_w_mode(chip_id, true, cc);

        // Read the CC value 
        int cc_readback = 0;
        rdram_reg_r_mode(chip_id, &cc_readback);
        rdram_reg_r_mode(chip_id, &cc_readback);

        float ccf = cc_readback;
        float err = fabsf(ccf - target_cc);
        if (cc == 0 || err < minerr) {
            minerr = err;
            mincc = cc;
        }
    }

    debugf("rdram_calibrate_current: ", mincc, (uint32_t)(minerr * 0x3F));

    // Write the CC value in automatic mode.
    rdram_reg_w_mode(chip_id, true, mincc);
}

void rdram_init(void)
{
    debugf("rdram_init: start");
    rdram_dump_regs();

    // Check if it's already initialized (after reset)
    if (*RI_SELECT) return;

    // Start current calibration. This is necessary to ensure the RAC outputs
    // the correct current value to talk to RDRAM chips.
    debugf("rdram_init: autocalibrate RAC output current");
    *RI_CONFIG = RI_CONFIG_AUTO_CALIBRATION;   // Turn on the RI auto current calibration
    wait(0x10000);                             // Wait for calibration
    *RI_CURRENT_LOAD = 0;                      // Apply the calibrated value
    
    // Activate communication with RDRAM chips. We can't do this before current calibration
    // as the chips might not be able to communicate correctly if the current is wrong.
    *RI_SELECT = RI_SELECT_RX_TX;

    // Reset chips. After the reset, all chips are turned off (DE=0 in MODE), and
    // mapped to device ID 0.
    debugf("rdram_init: reset RDRAM chips");
    *RI_MODE = RI_MODE_RESET;    wait(0x100);
    *RI_MODE = RI_MODE_STANDARD; wait(0x100);

    // Initialize RDRAM register access
    rdram_reg_init();

    // All chips are now configured with the same chip ID (32). We now change the ID of
    // the first one to 0, so that we can begin addressing it separately. Notice that this
    // works because when using non-broadcast mode, only the first chip that matches the ID
    // receives the command, even if there are multiple chips with the same ID.
    enum { INITIAL_COMMON_ID = RDRAM_MAX_DEVICE_ID };

    rdram_reg_w_deviceid(RDRAM_BROADCAST, INITIAL_COMMON_ID);
    rdram_reg_w(RDRAM_BROADCAST, RDRAM_REG_REF_ROW, 0);
    rdram_reg_w_deviceid(INITIAL_COMMON_ID, 0);
    rdram_calibrate_current(0);
    rdram_reg_w_deviceid(INITIAL_COMMON_ID, 2);
    rdram_calibrate_current(2);
    rdram_reg_w_deviceid(INITIAL_COMMON_ID, 4);
    rdram_calibrate_current(4);
    rdram_reg_w_deviceid(INITIAL_COMMON_ID, 6);
    rdram_calibrate_current(6);
    
    #if DEBUG
    //rdram_reg_w_mode(RDRAM_BROADCAST, true, 0x20); // auto current mode, max current
    uint32_t manu[8], devtype[8];
    for (int i=0; i<8; i++) {
        manu[i] = rdram_reg_r(i, RDRAM_REG_DEVICE_MANUFACTURER);
        devtype[i] = rdram_reg_r(i, RDRAM_REG_DEVICE_TYPE);
    }
    debugf("rdram_init: manufacturer: ", manu[0], manu[1], manu[2], manu[3], manu[4], manu[5], manu[6], manu[7]);
    debugf("rdram_init: device type: ", devtype[0], devtype[1], devtype[2], devtype[3], devtype[4], devtype[5], devtype[6], devtype[7]);
    // debugf("rdram_init: chip 0: manufacturer:");
    // static const char *manufacturers[] = { "<unknown>", "?", "?", "?", "?", "NEC", "?", "?" };
    // debugf(manufacturers[manu[0] >> 16]);
    #endif

    debugf("rdram_init: done");
    rdram_dump_regs();
}
