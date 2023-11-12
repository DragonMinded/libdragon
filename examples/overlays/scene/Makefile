BUILD_DIR=build
DSO_COMPRESS_LEVEL=0 #Store DSOs uncompressed for easier analysis of output
DSO_CXXFLAGS=-fvisibility=hidden #Hide symbols in DSOs
include $(N64_INST)/include/n64.mk

main_SRC = overlays_scene.cpp scene.cpp

scene_common_SRC = sprite.cpp background.cpp
bgtest_SRC = scene/bg_test.cpp
spritetest_SRC = scene/sprite_test.cpp

assets_png = $(wildcard assets/*.png)
assets_conv = $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite)))

all: overlays_scene.z64

MAIN_ELF_EXTERNS := $(BUILD_DIR)/overlays_scene.externs
DSO_MODULES = scene_common.dso scene/bg_test.dso scene/sprite_test.dso
DSO_LIST = $(addprefix filesystem/, $(DSO_MODULES))

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	@$(N64_MKSPRITE) -f RGBA16 --compress -o "$(dir $@)" "$<"
    
$(BUILD_DIR)/overlays_scene.dfs: $(assets_conv) $(DSO_LIST)
$(BUILD_DIR)/overlays_scene.elf: $(main_SRC:%.cpp=$(BUILD_DIR)/%.o) $(MAIN_ELF_EXTERNS)
$(MAIN_ELF_EXTERNS): $(DSO_LIST)
filesystem/scene_common.dso: $(scene_common_SRC:%.cpp=$(BUILD_DIR)/%.o)
filesystem/scene/bg_test.dso: $(bgtest_SRC:%.cpp=$(BUILD_DIR)/%.o)
filesystem/scene/sprite_test.dso: $(spritetest_SRC:%.cpp=$(BUILD_DIR)/%.o)

$(BUILD_DIR)/overlays_scene.msym: $(BUILD_DIR)/overlays_scene.elf

overlays_scene.z64: N64_ROM_TITLE="Actor Overlay Demo"
overlays_scene.z64: $(BUILD_DIR)/overlays_scene.dfs $(BUILD_DIR)/overlays_scene.msym

clean:
	rm -rf $(BUILD_DIR) filesystem $(DSO_LIST) overlays_scene.z64

-include $(wildcard $(BUILD_DIR)/*.d)
-include $(wildcard $(BUILD_DIR)/scene/*.d)

.PHONY: all clean
