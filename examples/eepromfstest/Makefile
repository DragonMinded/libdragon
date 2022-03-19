all: eepromfstest_4k.z64 eepromfstest_16k.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/eepromfstest.o

eepromfstest_4k.z64: N64_ROM_TITLE = "EEPROMFS Test 4k"
eepromfstest_4k.z64: N64_ROM_SAVETYPE = eeprom4k

eepromfstest_16k.z64: N64_ROM_TITLE = "EEPROMFS Test 16k"
eepromfstest_16k.z64: N64_ROM_SAVETYPE = eeprom16k

$(BUILD_DIR)/eepromfstest_4k.elf: $(OBJS)
$(BUILD_DIR)/eepromfstest_16k.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64 *.v64
.PHONY: clean

-include $(wildcard $(BUILD_DIR)/*.d)

