all: test.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

SRC = test.c
OBJS = $(SRC:%.c=$(BUILD_DIR)/%.o)
DEPS = $(SRC:%.c=$(BUILD_DIR)/%.d)

test.z64: N64_ROM_TITLE = "Video Test"
test.z64: $(BUILD_DIR)/test.dfs

$(BUILD_DIR)/test.dfs: $(wildcard filesystem/*)
$(BUILD_DIR)/test.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(DEPS)
