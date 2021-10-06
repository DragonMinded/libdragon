BUILD_DIR = build
include $(N64_INST)/include/n64.mk

.DEFAULT_GOAL := all

src = eepromfstest.c
objs = $(src:%.c=$(BUILD_DIR)/%.o)
deps = $(src:%.c=$(BUILD_DIR)/%.d)

all: eepromfstest_4k.z64 eepromfstest_16k.z64
.PHONY: all

eepromfstest_4k.z64: N64_ROM_TITLE = "EEPROMFS Test 4k"
eepromfstest_4k.z64: N64_ED64ROMCONFIGFLAGS = --savetype eeprom4k

eepromfstest_16k.z64: N64_ROM_TITLE = "EEPROMFS Test 16k"
eepromfstest_16k.z64: N64_ED64ROMCONFIGFLAGS = --savetype eeprom16k

$(BUILD_DIR)/eepromfstest_4k.elf: $(objs)
$(BUILD_DIR)/eepromfstest_16k.elf: $(objs)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(deps)
