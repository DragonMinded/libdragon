#define RI_MODE								((volatile uint32_t*)0xA4700000)
#define RI_CONFIG							((volatile uint32_t*)0xA4700004)
#define RI_CURRENT_LOAD						((volatile uint32_t*)0xA4700008)
#define RI_SELECT							((volatile uint32_t*)0xA470000C)
#define RI_REFRESH							((volatile uint32_t*)0xA4700010)
#define RI_LATENCY							((volatile uint32_t*)0xA4700014)
#define RI_RERROR							((volatile uint32_t*)0xA4700018)
#define RI_WERROR							((volatile uint32_t*)0xA470001C)

#define RI_CONFIG_AUTO_CALIBRATION			0x40

void rdram_dump_regs(void)
{
    debugf("RI_MODE: ", *RI_MODE);
    debugf("RI_CONFIG: ", *RI_CONFIG);
    debugf("RI_CURRENT_LOAD: ", *RI_CURRENT_LOAD);
    debugf("RI_SELECT: ", *RI_SELECT);
    debugf("RI_LATENCY: ", *RI_LATENCY);
}

void rdram_init(void)
{
    rdram_dump_regs();

    // Check if it's already initialized (after reset)
    if (*RI_SELECT) return;

    // Start auto calibration
    *RI_CONFIG = RI_CONFIG_AUTO_CALIBRATION;
    for (int i=0; i<16*16; i++) {
        wait(0x100);
        debugf("WAIT: ", i);
    }
}
