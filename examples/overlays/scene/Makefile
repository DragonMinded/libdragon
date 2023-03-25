BUILD_DIR=build
USO_PLF_BASE_DIR=$(BUILD_DIR)
USO_BASE_DIR=filesystem
USO_MODULES = scene_common.plf scene/bg_test.plf scene/sprite_test.plf
include $(N64_INST)/include/n64.mk

main_SRC = overlays_scene.cpp scene.cpp

scene_common_SRC = sprite.cpp background.cpp
bgtest_SRC = scene/bg_test.cpp
spritetest_SRC = scene/sprite_test.cpp

assets_png = $(wildcard assets/*.png)
assets_conv = $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite)))

all: overlays_scene.z64

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	@$(N64_MKSPRITE) -f RGBA16 --compress -o "$(dir $@)" "$<"
    
$(BUILD_DIR)/overlays_scene.dfs: $(assets_conv) $(USO_LIST)
$(BUILD_DIR)/overlays_scene.elf: $(main_SRC:%.cpp=$(BUILD_DIR)/%.o)
$(USO_PLF_BASE_DIR)/scene_common.plf: $(scene_common_SRC:%.cpp=$(BUILD_DIR)/%.o)
$(USO_PLF_BASE_DIR)/scene/bg_test.plf: $(bgtest_SRC:%.cpp=$(BUILD_DIR)/%.o)
$(USO_PLF_BASE_DIR)/scene/sprite_test.plf: $(spritetest_SRC:%.cpp=$(BUILD_DIR)/%.o)

overlays_scene.z64: N64_ROM_TITLE="Actor Overlay Demo"
overlays_scene.z64: $(BUILD_DIR)/overlays_scene.dfs 

clean:
	rm -rf $(BUILD_DIR) $(USO_LIST) overlays_scene.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
