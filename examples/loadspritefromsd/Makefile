BUILD_DIR=build
include $(N64_INST)/include/n64.mk

src = loadspritefromsd.c
assets_png = $(wildcard assets/*.png)

assets_conv = $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite)))

MKSPRITE_FLAGS ?=

all: loadspritefromsd.z64

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	@$(N64_MKSPRITE) -f RGBA16 -o "$(dir $@)" "$<"

$(BUILD_DIR)/loadspritefromsd.dfs: $(assets_conv)
$(BUILD_DIR)/loadspritefromsd.elf: $(src:%.c=$(BUILD_DIR)/%.o)

loadspritefromsd.z64: N64_ROM_TITLE="loadspritefromsd"
loadspritefromsd.z64: $(BUILD_DIR)/loadspritefromsd.dfs

clean:
	rm -rf $(BUILD_DIR) filesystem/ loadspritefromsd.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
