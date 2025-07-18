all: customfont.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

SRC = customfont.c
OBJS = $(SRC:%.c=$(BUILD_DIR)/%.o)
DEPS = $(SRC:%.c=$(BUILD_DIR)/%.d)

filesystem/libdragon-font.sprite: assets/libdragon-font.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) --tiles 16,16 -o $(dir $@) "$<"

customfont.z64: N64_ROM_TITLE = "customfont"
customfont.z64: $(BUILD_DIR)/customfont.dfs

$(BUILD_DIR)/customfont.dfs: filesystem/libdragon-font.sprite
$(BUILD_DIR)/customfont.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(DEPS)
