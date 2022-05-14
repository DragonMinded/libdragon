all: loadspritefromsd.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/loadspritefromsd.o

loadspritefromsd.z64: N64_ROM_TITLE = "loadspritefromsd"
loadspritefromsd.z64: $(BUILD_DIR)/loadspritefromsd.dfs

$(BUILD_DIR)/loadspritefromsd.dfs: $(wildcard filesystem/*)
$(BUILD_DIR)/loadspritefromsd.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(wildcard $(BUILD_DIR)/*.d)
