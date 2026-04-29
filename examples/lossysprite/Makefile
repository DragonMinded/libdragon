BUILD_DIR=build
include $(N64_INST)/include/n64.mk

src = lossysprite.c
assets_png = $(wildcard assets/*.png)

assets_conv = $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite)))

MKSPRITE_FLAGS ?= --lossy 75 --compress 3

all: lossysprite.z64

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [LSPR] $@"
	$(N64_MKSPRITE) $(MKSPRITE_FLAGS) -o filesystem "$<"

$(BUILD_DIR)/lossysprite.dfs: $(assets_conv)
$(BUILD_DIR)/lossysprite.elf: $(src:%.c=$(BUILD_DIR)/%.o)

lossysprite.z64: N64_ROM_TITLE="Lossy Sprite Demo"
lossysprite.z64: $(BUILD_DIR)/lossysprite.dfs

clean:
	rm -rf $(BUILD_DIR) filesystem lossysprite.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
