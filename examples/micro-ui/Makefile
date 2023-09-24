BUILD_DIR = build
include $(N64_INST)/include/n64.mk

assets_ttf = $(wildcard assets/*.ttf)
assets_png = $(wildcard assets/*.png)

assets_conv = $(addprefix filesystem/,$(notdir $(assets_ttf:%.ttf=%.font64))) \
              $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite)))

OBJS = $(BUILD_DIR)/micro-ui.o \
	$(BUILD_DIR)/lib/microui.o \
	$(BUILD_DIR)/lib/microuiN64.o

all: micro-ui.z64

filesystem/%.font64: assets/%.ttf
	@mkdir -p $(dir $@)
	@echo "    [FONT] $@"
	$(N64_MKFONT) $(MKFONT_FLAGS) -s 9 -o filesystem "$<"

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) $(MKSPRITE_FLAGS) -o filesystem "$<"

$(BUILD_DIR)/micro-ui.dfs: $(assets_conv)
$(BUILD_DIR)/micro-ui.elf: $(OBJS)

micro-ui.z64: N64_ROM_TITLE = "MicroUI N64"
micro-ui.z64: $(BUILD_DIR)/micro-ui.dfs 

clean:
	rm -rf $(BUILD_DIR) *.z64 filesystem

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean