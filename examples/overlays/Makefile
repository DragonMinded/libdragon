BUILD_DIR=build
USO_PLF_BASE_DIR=$(BUILD_DIR)
USO_BASE_DIR=filesystem
USO_MODULES = circle.plf triangle.plf n64brew.plf
include $(N64_INST)/include/n64.mk

main_SRC = overlays_actor.c

circle_SRC = circle.c
triangle_SRC = triangle.c
n64brew_SRC = n64brew.c

assets_png = $(wildcard assets/*.png)
assets_conv = $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite)))

all: overlays_actor.z64

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	@$(N64_MKSPRITE) -f RGBA16 --compress -o "$(dir $@)" "$<"
    
$(BUILD_DIR)/overlays_actor.dfs: $(assets_conv) $(USO_LIST)
$(BUILD_DIR)/overlays_actor.elf: $(main_SRC:%.c=$(BUILD_DIR)/%.o)
$(USO_PLF_BASE_DIR)/circle.plf: $(circle_SRC:%.c=$(BUILD_DIR)/%.o)
$(USO_PLF_BASE_DIR)/triangle.plf: $(triangle_SRC:%.c=$(BUILD_DIR)/%.o)
$(USO_PLF_BASE_DIR)/n64brew.plf: $(n64brew_SRC:%.c=$(BUILD_DIR)/%.o)

overlays_actor.z64: N64_ROM_TITLE="Actor Overlay Demo"
overlays_actor.z64: $(BUILD_DIR)/overlays_actor.dfs 

clean:
	rm -rf $(BUILD_DIR) $(ALL_USOS) overlays_actor.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
