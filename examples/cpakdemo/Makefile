all: cpakdemo.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk
N64_ROM_CONTROLLER1 = n64,pak=controller
N64_ROM_REGIONFREE = true

OBJS = $(BUILD_DIR)/cpakdemo.o

cpakdemo.z64: N64_ROM_TITLE = "ControllerPak Demo"

$(BUILD_DIR)/cpakdemo.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(wildcard $(BUILD_DIR)/*.d)
