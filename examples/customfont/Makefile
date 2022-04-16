all: customfont.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

SRC = customfont.c
OBJS = $(SRC:%.c=$(BUILD_DIR)/%.o)
DEPS = $(SRC:%.c=$(BUILD_DIR)/%.d)

customfont.z64: N64_ROM_TITLE = "customfont"
customfont.z64: $(BUILD_DIR)/customfont.dfs

$(BUILD_DIR)/customfont.dfs: $(wildcard filesystem/*)
	$(N64_ROOTDIR)/bin/mksprite 16 16 8 filesystem/libdragon-font.png filesystem/libdragon-font.sprite
	$(N64_MKDFS) $@ filesystem

$(BUILD_DIR)/customfont.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(DEPS)
