BUILD_DIR=build
# This example uses various preview APIs including SF64 and MID64
LIBDRAGON_PREVIEW = 2
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/audioplayer.o

assets_xm = $(wildcard assets/*.xm) $(wildcard assets/*.XM)
assets_ym = $(wildcard assets/*.ym) $(wildcard assets/*.YM)
assets_mid = $(wildcard assets/*.mid) $(wildcard assets/*.MID)
assets_sf2 = $(wildcard assets/*.sf2) $(wildcard assets/*.SF2)

assets_xm64 = $(patsubst assets/%.xm,filesystem/%.xm64,$(filter %.xm,$(assets_xm))) \
			   $(patsubst assets/%.XM,filesystem/%.xm64,$(filter %.XM,$(assets_xm)))
assets_ym64 = $(patsubst assets/%.ym,filesystem/%.ym64,$(filter %.ym,$(assets_ym))) \
			   $(patsubst assets/%.YM,filesystem/%.ym64,$(filter %.YM,$(assets_ym)))
assets_mid64 = $(patsubst assets/%.mid,filesystem/%.mid64,$(filter %.mid,$(assets_mid))) \
               $(patsubst assets/%.MID,filesystem/%.mid64,$(filter %.MID,$(assets_mid)))
assets_sf64 = $(patsubst assets/%.sf2,filesystem/%.sf64,$(filter %.sf2,$(assets_sf2))) \
               $(patsubst assets/%.SF2,filesystem/%.sf64,$(filter %.SF2,$(assets_sf2)))

assets_conv = $(assets_xm64) \
			  $(assets_ym64) \
              $(assets_mid64) \
              $(assets_sf64)

#AUDIOCONV_FLAGS ?= --xm-ext-samples filesystem/samples

all: audioplayer.z64

# Run audioconv64 on all XM/YM/MID files under assets/
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

filesystem/%.mid64: assets/%.mid
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	@$(N64_AUDIOCONV) $(AUDIOCONV_FLAGS) -o filesystem "$<"
filesystem/%.mid64: assets/%.MID
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	@$(N64_AUDIOCONV) $(AUDIOCONV_FLAGS) -o filesystem "$<"

# Hardcoded GM bank for MID64 playback (samples stream from ROM).
filesystem/%.sf64: assets/%.sf2
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	@$(N64_AUDIOCONV) --sf-compress 1 -o filesystem "$<"
filesystem/%.sf64: assets/%.SF2
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	@$(N64_AUDIOCONV) --sf-compress 1 -o filesystem "$<"

# As an example, activate compression darkness.ym. The file will be smaller
# but it will be impossible to seek it in the player.
filesystem/darkness.ym64: AUDIOCONV_FLAGS += --ym-compress true

$(BUILD_DIR)/audioplayer.dfs: $(assets_conv)
$(BUILD_DIR)/audioplayer.elf: $(OBJS)

audioplayer.z64: N64_ROM_TITLE="Audio Player"
audioplayer.z64: $(BUILD_DIR)/audioplayer.dfs

clean:
	rm -rf $(BUILD_DIR) audioplayer.z64 filesystem

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
