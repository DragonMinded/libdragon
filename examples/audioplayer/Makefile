BUILD_DIR=build
include $(N64_INST)/include/n64.mk

src = audioplayer.c
assets_xm1 = $(wildcard assets/*.xm)
assets_xm2 = $(wildcard assets/*.XM)
assets_conv = $(addprefix filesystem/,$(notdir $(assets_xm1:%.xm=%.xm64))) \
              $(addprefix filesystem/,$(notdir $(assets_xm2:%.XM=%.xm64)))

all: audioplayer.z64

# Run audioconv64 on all XM files under assets/
# We do this file by file, but we could even do it just once for the whole
# directory, because audioconv64 supports directory walking.
filesystem/%.xm64: assets/%.xm
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	@$(N64_AUDIOCONV) -o filesystem $<
filesystem/%.xm64: assets/%.XM
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	@$(N64_AUDIOCONV) -o filesystem $<

$(BUILD_DIR)/audioplayer.dfs: $(assets_conv)
$(BUILD_DIR)/audioplayer.elf: $(src:%.c=$(BUILD_DIR)/%.o)

audioplayer.z64: N64_ROM_TITLE="Audio Player"
audioplayer.z64: $(BUILD_DIR)/audioplayer.dfs

clean:
	rm -rf $(BUILD_DIR) audioplayer.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
