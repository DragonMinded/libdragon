BUILD_DIR=build
USO_ELF_DIR=$(BUILD_DIR)
V=1
include $(N64_INST)/include/n64.mk

main_SRC = overlays_scene.cpp scene.cpp

scene_common_SRC = sprite.cpp background.cpp
bgtest_SRC = scene/bg_test.cpp
spritetest_SRC = scene/sprite_test.cpp

assets_png = $(wildcard assets/*.png)
assets_conv = $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite)))

all: overlays_scene.z64

MAIN_ELF_EXTERNS := $(BUILD_DIR)/overlays_scene.externs
USO_MODULES = scene_common.uso scene/bg_test.uso scene/sprite_test.uso
USO_LIST = $(addprefix filesystem/, $(USO_MODULES))

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	@$(N64_MKSPRITE) -f RGBA16 --compress -o "$(dir $@)" "$<"
    
$(BUILD_DIR)/overlays_scene.dfs: $(assets_conv) $(USO_LIST)
$(BUILD_DIR)/overlays_scene.elf: $(main_SRC:%.cpp=$(BUILD_DIR)/%.o) $(MAIN_ELF_EXTERNS)
$(MAIN_ELF_EXTERNS): $(USO_LIST)
filesystem/scene_common.uso: $(scene_common_SRC:%.cpp=$(BUILD_DIR)/%.o)
filesystem/scene/bg_test.uso: $(bgtest_SRC:%.cpp=$(BUILD_DIR)/%.o)
filesystem/scene/sprite_test.uso: $(spritetest_SRC:%.cpp=$(BUILD_DIR)/%.o)

overlays_scene.z64: N64_ROM_TITLE="Actor Overlay Demo"
overlays_scene.z64: $(BUILD_DIR)/overlays_scene.dfs 

clean:
	rm -rf $(BUILD_DIR) $(USO_LIST) overlays_scene.z64

-include $(wildcard $(BUILD_DIR)/*.d)
-include $(wildcard $(BUILD_DIR)/scene/*.d)

.PHONY: all clean
