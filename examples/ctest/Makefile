all: ctest.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

SRC = ctest.c
OBJS = $(SRC:%.c=$(BUILD_DIR)/%.o)
DEPS = $(SRC:%.c=$(BUILD_DIR)/%.d)

ctest.z64: N64_ROM_TITLE = "Controller Test"

$(BUILD_DIR)/ctest.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(DEPS)
