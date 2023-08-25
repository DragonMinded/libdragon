TARGET=brew_volley
BUILD_DIR=build
FSYSTEM_DIR=filesystem
include $(N64_INST)/include/n64.mk

src = main.c
assets_xm = $(wildcard assets/*.xm)
assets_wav = $(wildcard assets/*.wav)
assets_png = $(wildcard assets/*.png)
assets_ttf = $(wildcard assets/*.ttf)

assets_conv = $(addprefix filesystem/,$(notdir $(assets_xm:%.xm=%.xm64))) \
              $(addprefix filesystem/,$(notdir $(assets_wav:%.wav=%.wav64))) \
			  $(addprefix filesystem/,$(notdir $(assets_ttf:%.ttf=%.font64))) \
              $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite)))

#N64_CFLAGS = -Wno-error
AUDIOCONV_FLAGS ?=
MKSPRITE_FLAGS ?=

all: $(TARGET).z64

filesystem/%.xm64: assets/%.xm
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	@$(N64_AUDIOCONV) $(AUDIOCONV_FLAGS) -o filesystem $<

filesystem/%.wav64: assets/%.wav
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	@$(N64_AUDIOCONV) --wav-compress 1 -o filesystem $<

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	@$(N64_MKSPRITE) --format RGBA32 $(MKSPRITE_FLAGS) -o filesystem "$<"
	
filesystem/%.font64: assets/%.ttf
	@mkdir -p $(dir $@)
	@echo "    [FONT] $@"
	@$(N64_MKFONT) $(MKFONT_FLAGS) -o filesystem "$<"

filesystem/n64brew.sprite: MKSPRITE_FLAGS=--format RGBA32 --tiles 32,32
filesystem/background.sprite: MKSPRITE_FLAGS=--format RGBA16 --tiles 32,32
filesystem/Pacifico.font64: MKFONT_FLAGS+=--size 32

$(BUILD_DIR)/$(TARGET).dfs: $(assets_conv)
$(BUILD_DIR)/$(TARGET).elf: $(src:%.c=$(BUILD_DIR)/%.o)

$(TARGET).z64: N64_ROM_TITLE="N64brew GameJam 4"
$(TARGET).z64: $(BUILD_DIR)/$(TARGET).dfs 

clean:
	rm -rf $(BUILD_DIR) $(FSYSTEM_DIR) $(TARGET).z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
