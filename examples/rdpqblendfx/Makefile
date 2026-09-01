LIBDRAGON_PREVIEW = 2		# blendfx still in preview
BUILD_DIR=build
include $(N64_INST)/include/n64.mk

src = rdpqblendfx.c
assets_png = $(wildcard assets/*.png)
assets_conv = $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite)))

all: rdpqblendfx.z64

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	@$(N64_MKSPRITE) -o filesystem "$<"

$(BUILD_DIR)/rdpqblendfx.dfs: $(assets_conv)
$(BUILD_DIR)/rdpqblendfx.elf: $(src:%.c=$(BUILD_DIR)/%.o)

rdpqblendfx.z64: N64_ROM_TITLE="RDPQ BlendFX"
rdpqblendfx.z64: $(BUILD_DIR)/rdpqblendfx.dfs

clean:
	rm -rf $(BUILD_DIR) rdpqblendfx.z64 filesystem

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
