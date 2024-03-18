all: cpak-utest.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/cpak-utest.o

cpak-utest.z64: N64_ROM_TITLE = "CPak Unit Test"

$(BUILD_DIR)/cpak-utest.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(wildcard $(BUILD_DIR)/*.d)
