all: joypadtest.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/joypadtest.o

joypadtest.z64: N64_ROM_TITLE = "Joypad Test"

$(BUILD_DIR)/joypadtest.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(wildcard $(BUILD_DIR)/*.d)
