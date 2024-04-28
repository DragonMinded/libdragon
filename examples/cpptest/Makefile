BUILD_DIR=build
SOURCE_DIR=.
include $(N64_INST)/include/n64.mk

all: cpptest.z64

OBJS = $(BUILD_DIR)/cpptest.o

$(BUILD_DIR)/cpptest.elf: $(OBJS)

cpptest.z64: N64_ROM_TITLE="C++ test"

clean:
	rm -rf $(BUILD_DIR) cpptest.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
