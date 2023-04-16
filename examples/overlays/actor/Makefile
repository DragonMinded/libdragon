BUILD_DIR=build
USO_ELF_DIR=$(BUILD_DIR)
include $(N64_INST)/include/n64.mk

main_SRC = overlays_actor.c

circle_SRC = circle.c
triangle_SRC = triangle.c
n64brew_SRC = n64brew.c

assets_png = $(wildcard assets/*.png)
assets_conv = $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite)))

all: overlays_actor.z64

MAIN_ELF_EXTERNS := $(BUILD_DIR)/overlays_actor.externs
USO_MODULES = circle.uso triangle.uso n64brew.uso
USO_LIST = $(addprefix filesystem/, $(USO_MODULES))

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	@$(N64_MKSPRITE) -f RGBA16 --compress -o "$(dir $@)" "$<"
    
$(BUILD_DIR)/overlays_actor.dfs: $(assets_conv) $(USO_LIST)
$(BUILD_DIR)/overlays_actor.elf: $(main_SRC:%.c=$(BUILD_DIR)/%.o) $(MAIN_ELF_EXTERNS)
$(MAIN_ELF_EXTERNS): $(USO_LIST)
filesystem/circle.uso: $(circle_SRC:%.c=$(BUILD_DIR)/%.o)
filesystem/triangle.uso: $(triangle_SRC:%.c=$(BUILD_DIR)/%.o)
filesystem/n64brew.uso: $(n64brew_SRC:%.c=$(BUILD_DIR)/%.o)

overlays_actor.z64: N64_ROM_TITLE="Actor Overlay Demo"
overlays_actor.z64: $(BUILD_DIR)/overlays_actor.dfs 

clean:
	rm -rf $(BUILD_DIR) $(USO_LIST) overlays_actor.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
