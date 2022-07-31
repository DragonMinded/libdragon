BUILD_DIR=build
include $(N64_INST)/include/n64.mk

src = gldemo.c
assets_png = $(wildcard assets/*.png)

assets_conv = $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite)))

MKSPRITE_FLAGS ?=

all: gldemo.z64

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	@$(N64_MKSPRITE) $(MKSPRITE_FLAGS) "$<" "$@"

filesystem/circle.sprite: MKSPRITE_FLAGS=16 1 1
filesystem/diamond.sprite: MKSPRITE_FLAGS=16 1 1
filesystem/pentagon.sprite: MKSPRITE_FLAGS=16 1 1
filesystem/triangle.sprite: MKSPRITE_FLAGS=16 1 1

$(BUILD_DIR)/gldemo.dfs: $(assets_conv)
$(BUILD_DIR)/gldemo.elf: $(src:%.c=$(BUILD_DIR)/%.o)

gldemo.z64: N64_ROM_TITLE="GL Demo"
gldemo.z64: $(BUILD_DIR)/gldemo.dfs

clean:
	rm -rf $(BUILD_DIR) gldemo.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
