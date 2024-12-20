BUILD_DIR=build
include $(N64_INST)/include/n64.mk

src = mgdemo.c
assets_png = $(wildcard assets/*.png)
assets_glb = $(wildcard assets/*.glb)

assets_conv = $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite))) \
			  $(addprefix filesystem/,$(notdir $(assets_glb:%.glb=%.model64)))

MKSPRITE_FLAGS ?=

all: mgdemo.z64

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	@$(N64_MKSPRITE) --compress -o "$(dir $@)" "$<"

filesystem/%.model64: assets/%.glb
	@mkdir -p $(dir $@)
	@echo "    [MODEL] $@"
	@$(N64_MGFX) -o "$(dir $@)" "$<"

$(BUILD_DIR)/mgdemo.dfs: $(assets_conv)
$(BUILD_DIR)/mgdemo.elf: $(src:%.c=$(BUILD_DIR)/%.o)

mgdemo.z64: N64_ROM_TITLE="Magma Demo"
mgdemo.z64: $(BUILD_DIR)/mgdemo.dfs

clean:
	rm -rf $(BUILD_DIR) filesystem/ mgdemo.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
