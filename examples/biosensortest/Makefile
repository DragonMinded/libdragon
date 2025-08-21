all: biosensortest.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/biosensortest.o

biosensortest.z64: N64_ROM_TITLE = "Bio Sensor Test"

$(BUILD_DIR)/biosensortest.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(wildcard $(BUILD_DIR)/*.d)
