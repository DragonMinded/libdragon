BUILD_DIR=build
include n64.mk

all: ucodetest.z64

$(BUILD_DIR)/ucodetest.elf: \
	$(BUILD_DIR)/ucodetest.o \
	$(BUILD_DIR)/rsp_basic.o

ucodetest.z64: N64_ROM_TITLE="UCode Test"

clean:
	rm -rf $(BUILD_DIR) ucodetest.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
