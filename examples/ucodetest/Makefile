all: ucodetest.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/ucodetest.o $(BUILD_DIR)/rsp_basic.o

ucodetest.z64: N64_ROM_TITLE = "UCode Test"

$(BUILD_DIR)/ucodetest.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(wildcard $(BUILD_DIR)/*.d))