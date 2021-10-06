BUILD_DIR = build
include $(N64_INST)/include/n64.mk

.DEFAULT_GOAL := spritemap.z64

src = spritemap.c
objs = $(src:%.c=$(BUILD_DIR)/%.o)
deps = $(src:%.c=$(BUILD_DIR)/%.d)

spritemap.z64: N64_ROM_TITLE = "Spritemap"
spritemap.z64: $(BUILD_DIR)/spritemap.dfs

$(BUILD_DIR)/spritemap.dfs: $(wildcard filesystem/*)
$(BUILD_DIR)/spritemap.elf: $(objs)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(deps)
