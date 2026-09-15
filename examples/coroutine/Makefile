BUILD_DIR=build
SOURCE_DIR=.
include $(N64_INST)/include/n64.mk

N64_CXXFLAGS += -std=gnu++20 -fno-exceptions -Os

all: coroutine.z64

OBJS = $(BUILD_DIR)/coroutine.o

$(BUILD_DIR)/coroutine.elf: $(OBJS)

coroutine.z64: N64_ROM_TITLE="coroutine test"

clean:
	rm -rf $(BUILD_DIR) coroutine.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
