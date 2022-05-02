BUILD_DIR=build
include $(N64_INST)/include/n64.mk

src = rdpqdemo.c
assets_xm = $(wildcard assets/*.xm)
assets_wav = $(wildcard assets/*.wav)
assets_png = $(wildcard assets/*.png)

assets_conv = $(addprefix filesystem/,$(notdir $(assets_xm:%.xm=%.xm64))) \
              $(addprefix filesystem/,$(notdir $(assets_wav:%.wav=%.wav64))) \
              $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite)))

AUDIOCONV_FLAGS ?=
MKSPRITE_FLAGS ?=

all: rdpqdemo.z64

filesystem/%.xm64: assets/%.xm
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	@$(N64_AUDIOCONV) $(AUDIOCONV_FLAGS) -o filesystem $<

filesystem/%.wav64: assets/%.wav
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	@$(N64_AUDIOCONV) -o filesystem $<

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	@$(N64_MKSPRITE) $(MKSPRITE_FLAGS) "$<" "$@"

filesystem/n64brew.sprite: MKSPRITE_FLAGS=16 2 3
filesystem/tiles.sprite: MKSPRITE_FLAGS=16 2 2

$(BUILD_DIR)/rdpqdemo.dfs: $(assets_conv)
$(BUILD_DIR)/rdpqdemo.elf: $(src:%.c=$(BUILD_DIR)/%.o)

rdpqdemo.z64: N64_ROM_TITLE="RSPQ Demo"
rdpqdemo.z64: $(BUILD_DIR)/rdpqdemo.dfs 

clean:
	rm -rf $(BUILD_DIR) rdpqdemo.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
