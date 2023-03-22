BUILD_DIR=build
USO_PLF_BASE_DIR=$(BUILD_DIR)
USO_BASE_DIR=filesystem
USO_MODULES = circle.plf triangle.plf n64brew.plf
include $(N64_INST)/include/n64.mk

main_SRC = overlays.c

circle_SRC = circle.c
triangle_SRC = triangle.c
n64brew_SRC = n64brew.c

assets_png = $(wildcard assets/*.png)
assets_conv = $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite)))

all: overlays.z64

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	@$(N64_MKSPRITE) -f RGBA16 --compress -o "$(dir $@)" "$<"
    
$(BUILD_DIR)/overlays.dfs: $(assets_conv) $(USO_LIST)
$(BUILD_DIR)/overlays.elf: $(main_SRC:%.c=$(BUILD_DIR)/%.o)
$(USO_PLF_BASE_DIR)/circle.plf: $(circle_SRC:%.c=$(BUILD_DIR)/%.o)
$(USO_PLF_BASE_DIR)/triangle.plf: $(triangle_SRC:%.c=$(BUILD_DIR)/%.o)
$(USO_PLF_BASE_DIR)/n64brew.plf: $(n64brew_SRC:%.c=$(BUILD_DIR)/%.o)

overlays.z64: N64_ROM_TITLE="Overlay Demo"
overlays.z64: $(BUILD_DIR)/overlays.dfs 

clean:
	rm -rf $(BUILD_DIR) $(ALL_USOS) overlays.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
