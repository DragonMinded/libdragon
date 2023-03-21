BUILD_DIR=build
USO_ELF_BASE_DIR=$(BUILD_DIR)
USO_BASE_DIR=filesystem
include ovl_n64.mk

main_SRC = overlays.c

ALL_MODULES :=

ALL_MODULES += circle.elf
circle_SRC = circle.c
ALL_MODULES += triangle.elf
triangle_SRC = triangle.c
ALL_MODULES += n64brew.elf
n64brew_SRC = n64brew.c

ALL_USOS := $(addprefix $(USO_BASE_DIR)/, $(addsuffix .uso, $(basename $(ALL_MODULES))))

assets_png = $(wildcard assets/*.png)
assets_conv = $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite)))

all: overlays.z64

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	@$(N64_MKSPRITE) -f RGBA16 --compress -o "$(dir $@)" "$<"
    
$(BUILD_DIR)/overlays.dfs: $(assets_conv) $(ALL_USOS)
$(BUILD_DIR)/overlays.elf: $(main_SRC:%.c=$(BUILD_DIR)/%.o)
$(USO_ELF_BASE_DIR)/circle.elf: $(circle_SRC:%.c=$(BUILD_DIR)/%.o)
$(USO_ELF_BASE_DIR)/triangle.elf: $(triangle_SRC:%.c=$(BUILD_DIR)/%.o)
$(USO_ELF_BASE_DIR)/n64brew.elf: $(n64brew_SRC:%.c=$(BUILD_DIR)/%.o)

overlays.z64: N64_ROM_TITLE="Overlay Demo"
overlays.z64: $(BUILD_DIR)/overlays.dfs 

clean:
	rm -rf $(BUILD_DIR) $(ALL_USOS) overlays.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
