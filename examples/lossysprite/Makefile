BUILD_DIR=build
include $(N64_INST)/include/n64.mk

src = lossysprite.c
assets_png = $(wildcard assets/*.png)

assets_conv = $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite)))

all: lossysprite.z64

# Lossily-compress the background sprites
# Photos are royalty-free images under the Pexels license: https://www.pexels.com/license/
filesystem/bg0.sprite: MKSPRITE_FLAGS=--format RGBA16 --lossy 75 --compress 3
filesystem/bg1.sprite: MKSPRITE_FLAGS=--format RGBA16 --lossy 75 --compress 3
filesystem/bg2.sprite: MKSPRITE_FLAGS=--format RGBA16 --lossy 75 --compress 3
filesystem/bg3.sprite: MKSPRITE_FLAGS=--format RGBA16 --lossy 75 --compress 3
filesystem/bg4.sprite: MKSPRITE_FLAGS=--format RGBA16 --lossy 75 --compress 3
filesystem/bg5.sprite: MKSPRITE_FLAGS=--format RGBA16 --lossy 75 --compress 3
filesystem/bg6.sprite: MKSPRITE_FLAGS=--format RGBA16 --lossy 75 --compress 3
# Losslessly-compress the brew sprite in standard RGBA16 format
filesystem/n64brew.sprite: MKSPRITE_FLAGS=--format RGBA16 --compress

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) $(MKSPRITE_FLAGS) -o filesystem "$<"

$(BUILD_DIR)/lossysprite.dfs: $(assets_conv)
$(BUILD_DIR)/lossysprite.elf: $(src:%.c=$(BUILD_DIR)/%.o)

lossysprite.z64: N64_ROM_TITLE="Lossy Sprite Demo"
lossysprite.z64: $(BUILD_DIR)/lossysprite.dfs

clean:
	rm -rf $(BUILD_DIR) filesystem lossysprite.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
