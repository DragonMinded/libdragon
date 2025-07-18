BUILD_DIR=build
include $(N64_INST)/include/n64.mk

src = spriteanim.c
assets_png = $(wildcard assets/*.png)

assets_conv = $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite)))


AUDIOCONV_FLAGS ?=
MKSPRITE_FLAGS ?=

all: spriteanim.z64

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	@$(N64_MKSPRITE) $(MKSPRITE_FLAGS) -o filesystem "$<"

$(BUILD_DIR)/spriteanim.dfs: $(assets_conv)
$(BUILD_DIR)/spriteanim.elf: $(src:%.c=$(BUILD_DIR)/%.o)

spriteanim.z64: N64_ROM_TITLE="Sprite Anim Demo"
spriteanim.z64: $(BUILD_DIR)/spriteanim.dfs 

clean:
	rm -rf $(BUILD_DIR) spriteanim.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
