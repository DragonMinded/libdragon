all: savedoodle_eeprom4k.z64 savedoodle_eeprom16k.z64 savedoodle_sram256k.z64 \
     savedoodle_sram768k.z64 savedoodle_sram1m.z64 savedoodle_flashram.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/savedoodle.o

# One ROM per save type: same program, different N64_ROM_SAVETYPE so emulators
# and flashcarts mount the matching backing store. The program detects at
# runtime which chip is actually present.
savedoodle_eeprom4k.z64:  N64_ROM_TITLE = "Doodle EEPROM 4k"
savedoodle_eeprom4k.z64:  N64_ROM_SAVETYPE = eeprom4k

savedoodle_eeprom16k.z64: N64_ROM_TITLE = "Doodle EEPROM 16k"
savedoodle_eeprom16k.z64: N64_ROM_SAVETYPE = eeprom16k

savedoodle_sram256k.z64:  N64_ROM_TITLE = "Doodle SRAM 256k"
savedoodle_sram256k.z64:  N64_ROM_SAVETYPE = sram256k

savedoodle_sram768k.z64:  N64_ROM_TITLE = "Doodle SRAM 768k"
savedoodle_sram768k.z64:  N64_ROM_SAVETYPE = sram768k

savedoodle_sram1m.z64:    N64_ROM_TITLE = "Doodle SRAM 1m"
savedoodle_sram1m.z64:    N64_ROM_SAVETYPE = sram1m

savedoodle_flashram.z64:  N64_ROM_TITLE = "Doodle FlashRAM"
savedoodle_flashram.z64:  N64_ROM_SAVETYPE = flashram

$(BUILD_DIR)/savedoodle_eeprom4k.elf: $(OBJS)
$(BUILD_DIR)/savedoodle_eeprom16k.elf: $(OBJS)
$(BUILD_DIR)/savedoodle_sram256k.elf: $(OBJS)
$(BUILD_DIR)/savedoodle_sram768k.elf: $(OBJS)
$(BUILD_DIR)/savedoodle_sram1m.elf: $(OBJS)
$(BUILD_DIR)/savedoodle_flashram.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64 *.v64
.PHONY: clean

-include $(wildcard $(BUILD_DIR)/*.d)
