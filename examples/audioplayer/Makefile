BUILD_DIR=build
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/audioplayer.o

assets_xm1 = $(wildcard assets/*.xm)
assets_xm2 = $(wildcard assets/*.XM)
assets_ym1 = $(wildcard assets/*.ym)
assets_ym2 = $(wildcard assets/*.YM)
assets_conv = $(addprefix filesystem/,$(notdir $(assets_xm1:%.xm=%.xm64))) \
              $(addprefix filesystem/,$(notdir $(assets_xm2:%.XM=%.xm64))) \
              $(addprefix filesystem/,$(notdir $(assets_ym1:%.ym=%.ym64))) \
              $(addprefix filesystem/,$(notdir $(assets_ym2:%.YM=%.ym64)))

AUDIOCONV_FLAGS ?=

all: audioplayer.z64

# Run audioconv64 on all XM/YM files under assets/
# We do this file by file, but we could even do it just once for the whole
# directory, because audioconv64 supports directory walking.
filesystem/%.xm64: assets/%.xm
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	@$(N64_AUDIOCONV) $(AUDIOCONV_FLAGS) -o filesystem "$<"
filesystem/%.xm64: assets/%.XM
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	@$(N64_AUDIOCONV) $(AUDIOCONV_FLAGS) -o filesystem "$<"

filesystem/%.ym64: assets/%.ym
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	$(N64_AUDIOCONV) $(AUDIOCONV_FLAGS) -o filesystem "$<"
filesystem/%.ym64: assets/%.YM
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	$(N64_AUDIOCONV) $(AUDIOCONV_FLAGS) -o filesystem "$<"

# As an example, activate compression darkness.ym. The file will be smaller
# but it will be impossible to seek it in the player.
filesystem/darkness.ym64: AUDIOCONV_FLAGS += --ym-compress true

$(BUILD_DIR)/audioplayer.dfs: $(assets_conv)
$(BUILD_DIR)/audioplayer.elf: $(OBJS)

audioplayer.z64: N64_ROM_TITLE="Audio Player"
audioplayer.z64: $(BUILD_DIR)/audioplayer.dfs

clean:
	rm -rf $(BUILD_DIR) audioplayer.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
