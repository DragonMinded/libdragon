#define RI_MODE								((volatile uint32_t*)0xA4700000)
#define RI_CONFIG							((volatile uint32_t*)0xA4700004)
#define RI_CURRENT_LOAD						((volatile uint32_t*)0xA4700008)
#define RI_SELECT							((volatile uint32_t*)0xA470000C)
#define RI_REFRESH							((volatile uint32_t*)0xA4700010)
#define RI_LATENCY							((volatile uint32_t*)0xA4700014)
#define RI_RERROR							((volatile uint32_t*)0xA4700018)
#define RI_WERROR							((volatile uint32_t*)0xA470001C)

#define RI_CONFIG_AUTO_CALIBRATION			0x40
#define RI_SELECT_RX_TX						0x14
#define RI_MODE_CLOCK_RX                    0x8
#define RI_MODE_CLOCK_TX                    0x8
#define RI_MODE_RESET						0x0
#define RI_MODE_STANDARD					(0x2 | RI_MODE_CLOCK_RX | RI_MODE_CLOCK_TX)

static void rdram_dump_regs(void)
{
    debugf("\tRI_MODE: ", *RI_MODE);
    debugf("\tRI_CONFIG: ", *RI_CONFIG);
    debugf("\tRI_CURRENT_LOAD: ", *RI_CURRENT_LOAD);
    debugf("\tRI_SELECT: ", *RI_SELECT);
    debugf("\tRI_LATENCY: ", *RI_LATENCY);
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
    // as the chips might be able to communicate correctly if the current is wrong.
    *RI_SELECT = RI_SELECT_RX_TX;

    // Now that we can read back from the chips, we can read the calibrated current
    debugf("\tCalibrated current: ", *RI_CURRENT_LOAD);

    // Reset chips
    debugf("rdram_init: reset chips");
    *RI_MODE = RI_MODE_RESET;    wait(0x100);
    *RI_MODE = RI_MODE_STANDARD; wait(0x100);

    debugf("rdram_init: done");
    rdram_dump_regs();
}
