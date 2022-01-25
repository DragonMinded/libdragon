BUILD_DIR=build
include $(N64_INST)/include/n64.mk

src = videoplayer.c

all: videoplayer.z64

$(BUILD_DIR)/videoplayer.dfs: filesystem/*
$(BUILD_DIR)/videoplayer.elf: $(src:%.c=$(BUILD_DIR)/%.o)

videoplayer.z64: N64_ROM_TITLE="Video Player"
videoplayer.z64: $(BUILD_DIR)/videoplayer.dfs

clean:
	rm -rf $(BUILD_DIR) videoplayer.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
