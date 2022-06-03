BUILD_DIR=build
include $(N64_INST)/include/n64.mk

all: gldemo.z64

$(BUILD_DIR)/gldemo.elf: $(BUILD_DIR)/gldemo.o

gldemo.z64: N64_ROM_TITLE="GL Demo"

clean:
	rm -rf $(BUILD_DIR) gldemo.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
