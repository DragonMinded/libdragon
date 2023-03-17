BUILD_DIR=build
include $(N64_INST)/include/n64.mk

src = overlays.c

assets_png = $(wildcard assets/*.png)

assets_conv = $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite)))

USO_LIST := filesystem/circle.uso \
filesystem/triangle.uso \
filesystem/n64brew.uso

all: overlays.z64

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	@$(N64_MKSPRITE) -f RGBA16 --compress -o "$(dir $@)" "$<"
    
$(BUILD_DIR)/overlays.dfs: $(assets_conv) $(USO_LIST)
$(BUILD_DIR)/overlays.elf: $(src:%.c=$(BUILD_DIR)/%.o)

overlays.z64: N64_ROM_TITLE="Overlay Demo"
overlays.z64: $(BUILD_DIR)/overlays.dfs 

clean:
	rm -rf $(BUILD_DIR) overlays.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
