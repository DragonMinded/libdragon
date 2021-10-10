all: spritemap.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

SRC = spritemap.c
OBJS = $(SRC:%.c=$(BUILD_DIR)/%.o)
DEPS = $(SRC:%.c=$(BUILD_DIR)/%.d)

spritemap.z64: N64_ROM_TITLE = "Spritemap"
spritemap.z64: $(BUILD_DIR)/spritemap.dfs

$(BUILD_DIR)/spritemap.dfs: $(wildcard filesystem/*)
$(BUILD_DIR)/spritemap.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(DEPS)
