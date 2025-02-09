all: vifx.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

assets_png = $(wildcard assets/*.png)
assets_conv = $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite)))
OBJS = $(BUILD_DIR)/vifx.o

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	@$(N64_MKSPRITE) $(MKSPRITE_FLAGS) -o filesystem "$<"

vifx.z64: N64_ROM_TITLE = "VI Effects"
vifx.z64: $(BUILD_DIR)/vifx.dfs 
$(BUILD_DIR)/vifx.dfs: $(assets_conv)
$(BUILD_DIR)/vifx.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(wildcard $(BUILD_DIR)/*.d)
