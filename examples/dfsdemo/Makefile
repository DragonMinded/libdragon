all: dfsdemo.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/dfsdemo.o

dfsdemo.z64: N64_ROM_TITLE = "DragonFS Demo"
dfsdemo.z64: $(BUILD_DIR)/dfsdemo.dfs

$(BUILD_DIR)/dfsdemo.dfs: $(wildcard filesystem/*)
$(BUILD_DIR)/dfsdemo.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(wildcard $(BUILD_DIR)/*.d)
