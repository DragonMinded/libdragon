BUILD_DIR=build
include n64.mk

src = spritemap.c

all: spritemap.z64

spritemap.dfs: $(wildcard filesystem/*)
spritemap.z64: N64_ROM_TITLE="Spritemap"
spritemap.z64: spritemap.dfs 
spritemap.elf: $(src:%.c=$(BUILD_DIR)/%.o)

clean:
	rm -f $(BUILD_DIR)/* spritemap.dfs spritemap.elf spritemap.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
