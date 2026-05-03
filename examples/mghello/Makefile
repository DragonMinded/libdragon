BUILD_DIR=build
include $(N64_INST)/include/n64.mk

src = mghello.c

MKSPRITE_FLAGS ?=

all: mghello.z64

$(BUILD_DIR)/mghello.elf: $(src:%.c=$(BUILD_DIR)/%.o)

mghello.z64: N64_ROM_TITLE="Magma Hello"

clean:
	rm -rf $(BUILD_DIR) mghello.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
