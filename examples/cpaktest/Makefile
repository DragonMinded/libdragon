all: cpaktest.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/cpaktest.o

cpaktest.z64: N64_ROM_TITLE = "ControllerPak Test"

$(BUILD_DIR)/cpaktest.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(wildcard $(BUILD_DIR)/*.d)
