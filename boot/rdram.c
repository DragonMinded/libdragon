/** 
 * Change to 1 to activate support for obsolete HW1 RCP.
 * This version has a different layout for the RDRAM registers.
 * No known commercial unit is using this, so it is disabled by
 * default.
 */
#define SUPPORT_HW1   0

#define MI_MODE                             ((volatile uint32_t*)0xA4300000)
#define MI_WMODE_CLEAR_INIT_MODE            0x80
#define MI_WMODE_SET_INIT_MODE              0x100
#define MI_WMODE_LENGTH(n)                  ((n)-1)
#define MI_WMODE_SET_RDRAM_REG_MODE         0x2000
#define MI_WMODE_CLEAR_RDRAM_REG_MODE       0x1000
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

static void rdram_reg_init(void) {
    // On RI v1, registers are 0x200 bytes apart
    uint32_t version = SUPPORT_HW1 ? mi_version_io() : 2;
    debugf("rdram_reg_init: IO version ", version);
    switch (version) {
    case 1:  rdram_reg_stride_shift =  9-2; break;
    default: rdram_reg_stride_shift = 10-2; break;
    }
}

static void rdram_reg_w(int chip_id, rdram_reg_t reg, uint32_t value)
{
    // RDRAM registers are physically little-endian. Swap them on write, so that
    // the rest of the code follows the datasheets.
    value = byteswap32(value);
    if (chip_id < 0) {
        RDRAM_REGS_BROADCAST[reg] = value;
        debugf("rdram_reg_w: ", (uint32_t)(&RDRAM_REGS_BROADCAST[reg]), value);
    } else {
        RDRAM_REGS[(chip_id << rdram_reg_stride_shift) + reg] = value;
        debugf("rdram_reg_w: ", (uint32_t)(&RDRAM_REGS[(chip_id << rdram_reg_stride_shift) + reg]), value);
    }
}

static uint32_t rdram_reg_r(int chip_id, rdram_reg_t reg)
{
    *MI_MODE = MI_WMODE_SET_RDRAM_REG_MODE;
    uint32_t value = RDRAM_REGS[(chip_id << rdram_reg_stride_shift) + reg];
    *MI_MODE = MI_WMODE_CLEAR_RDRAM_REG_MODE;
    return byteswap32(value);
}

static void rdram_reg_w_deviceid(int chip_id, uint16_t new_chip_id)
{   
    uint32_t value = 0;

    value |= ((new_chip_id >>  0) & 0x03F) <<  3;   // Bits 0..5
    value |= ((new_chip_id >>  6) & 0x1FF) << 15;   // Bits 6..14
    value |= ((new_chip_id >> 15) & 0x001) << 31;   // Bit  15

    rdram_reg_w(chip_id, RDRAM_REG_DEVICE_ID, value);
}

static void rdram_reg_w_delay(int chip_id, uint32_t delay)
{
    // Delay register uses the special MI "init mode" that basically repeats
    // the same word multiple times with a burst write to RDRAM chips. This is
    // probably required because RI doesn't have configurable delay timings, so
    // a burst is the best way to make sure the value does get written at boot.
    // After the corect value is written, RI and RDRAM match the timings, so
    // everything should work fine.
    // NOTE: MI init mode auto-resets itself after the first write, so there's
    // not need to explicitly clear it.
    *MI_MODE = MI_WMODE_SET_INIT_MODE | MI_WMODE_LENGTH(0x10);
    rdram_reg_w(chip_id, RDRAM_REG_DELAY, delay);
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

void rdram_calibrate_current(uint16_t chip_id, uint32_t *vaddr)
{
    float weighted_sum = 0.0f;
    float prev_accuracy = 0.0f;

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

    int target_cc = weighted_sum * 2.2f + 0.5f;
    debugf("rdram_calibrate_current: target_cc ", target_cc);
}

void rdram_init(void)
{
    debugf("rdram_init: start");
    rdram_dump_regs();

    // Check if it's already initialized (after reset)
    if (*RI_SELECT) return;

    // Start current calibration. Quoting the datasheet:
    //    The Rambus Channel output driveers are open-drain current sinks.
    //    They are designed to sink the amount of current required to provide the
    //    proper voltage swing on a terminated transmission line (the Channel) with
    //    a characteristic impedance that may vary from system to system.
    //    In order to work with all systems variations, the output drivers have
    //    been designed to be calibrated for each system. Each output
    //    driver is constructed from six binary weighted transistors that are
    //    driven in combination to provide the desired output current.
    //    Because the drive current of the individual transistors will vary
    //    from RAC to RAC, and because the drive current will change with voltage
    //    and temperature, internal calibration circuitry is provided to
    //    help set and maintain constant current output over time.

    debugf("rdram_init: autocalibrate current");
    *RI_CONFIG = RI_CONFIG_AUTO_CALIBRATION;   // Turn on the RI auto current calibration
    wait(0x10000);          // Wait for propagation
    *RI_CURRENT_LOAD = 0;   // (?) Load the value calibrated by RI into the RAMDAC (any value here will do)
    
    // Activate communication with RDRAM chips. We can't do this before current calibration
    // as the chips might not be able to communicate correctly if the current is wrong.
    *RI_SELECT = RI_SELECT_RX_TX;

    // Now that we can read back from the chips, we can read the calibrated current
    debugf("\tCalibrated current: ", *RI_CURRENT_LOAD);

    // Reset chips
    debugf("rdram_init: reset chips");
    *RI_MODE = RI_MODE_RESET;    wait(0x100);
    *RI_MODE = RI_MODE_STANDARD; wait(0x100);

    // Initialize chips
    rdram_reg_init();
    rdram_reg_w_delay(RDRAM_BROADCAST, byteswap32(0x18082838));

    // All chips are now configured with the same chip ID (32). We now change the ID of
    // the first one to 0, so that we can begin addressing it separately. Notice that this
    // works because when using non-broadcast mode, only the first chip that matches the ID
    // receives the command, even if there are multiple chips with the same ID.
    enum { INITIAL_COMMON_ID = 32 };  // FIXME: why this must be 32? Smaller values don't seem to work
    rdram_reg_w_deviceid(RDRAM_BROADCAST, INITIAL_COMMON_ID);
    rdram_reg_w_deviceid(INITIAL_COMMON_ID, 0);
    rdram_calibrate_current(0, RDRAM);
    rdram_reg_w_deviceid(INITIAL_COMMON_ID, 2);
    rdram_calibrate_current(2, RDRAM + 2*1024*1024);
    rdram_reg_w_deviceid(INITIAL_COMMON_ID, 4);
    rdram_calibrate_current(4, RDRAM + 4*1024*1024);
    
    #if 1
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
