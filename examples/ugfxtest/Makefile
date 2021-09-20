BUILD_DIR=build
include n64.mk

all: ugfxtest.z64

$(BUILD_DIR)/spritemap.dfs: \
	filesystem/test.sprite
$(BUILD_DIR)/ugfxtest.elf: \
	$(BUILD_DIR)/ugfxtest.o

ugfxtest.z64: N64_ROM_TITLE="UGFX Test"
ugfxtest.z64: $(BUILD_DIR)/spritemap.dfs

clean:
	rm -rf $(BUILD_DIR) ugfxtest.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
