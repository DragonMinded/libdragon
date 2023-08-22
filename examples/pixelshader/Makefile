BUILD_DIR=build
include $(N64_INST)/include/n64.mk

src = pixelshader.c
asm = rsp_blend.S
assets_ttf = $(wildcard assets/*.ttf)
assets_png = $(wildcard assets/*.png)

assets_conv = $(addprefix filesystem/,$(notdir $(assets_ttf:%.ttf=%.font64))) \
              $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite)))

MKSPRITE_FLAGS ?= --compress
MKFONT_FLAGS ?= --compress --verbose --size 20 --range 20-7F

all: pixelshader.z64

filesystem/%.font64: assets/%.ttf
	@mkdir -p $(dir $@)
	@echo "    [FONT] $@"
	$(N64_MKFONT) $(MKFONT_FLAGS) -o filesystem "$<"

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) $(MKSPRITE_FLAGS) -o filesystem "$<"

filesystem/ComicSans.font64: MKFONT_FLAGS += -v

$(BUILD_DIR)/pixelshader.dfs: $(assets_conv)
$(BUILD_DIR)/pixelshader.elf: $(src:%.c=$(BUILD_DIR)/%.o) $(asm:%.S=$(BUILD_DIR)/%.o)

pixelshader.z64: N64_ROM_TITLE="Pixel Shader Demo"
pixelshader.z64: $(BUILD_DIR)/pixelshader.dfs 

clean:
	rm -rf $(BUILD_DIR) filesystem pixelshader.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
