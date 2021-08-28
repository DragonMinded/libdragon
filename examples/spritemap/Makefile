BUILD_DIR=build
include n64.mk

src = spritemap.c

all: spritemap.z64

$(BUILD_DIR)/spritemap.dfs: $(wildcard filesystem/*)
$(BUILD_DIR)/spritemap.elf: $(src:%.c=$(BUILD_DIR)/%.o)

spritemap.z64: N64_ROM_TITLE="Spritemap"
spritemap.z64: $(BUILD_DIR)/spritemap.dfs

clean:
	rm -rf $(BUILD_DIR) spritemap.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
