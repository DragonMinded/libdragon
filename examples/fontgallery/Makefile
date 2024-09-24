BUILD_DIR=build
include $(N64_INST)/include/n64.mk

src = fontgallery.c
assets_fnt = $(wildcard assets/*.fnt)
assets_ttf = $(wildcard assets/*.ttf)
assets_png = $(wildcard assets/*.png)
assets_txt = $(wildcard assets/*.txt)

assets_conv = $(addprefix filesystem/,$(notdir $(assets_ttf:%.ttf=%.font64))) \
			  $(addprefix filesystem/,$(notdir $(assets_fnt:%.fnt=%.font64))) \
              $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite))) \
			  $(addprefix filesystem/,$(notdir $(assets_txt:%.txt=%.txt)))

MKSPRITE_FLAGS ?=
MKFONT_FLAGS ?= --range all

all: fontgallery.z64

filesystem/%.txt: assets/%.txt
	@mkdir -p $(dir $@)
	@cp "$<" "$@"

filesystem/%.font64: assets/%.ttf
	@mkdir -p $(dir $@)
	@echo "    [FONT] $@"
	$(N64_MKFONT) $(MKFONT_FLAGS) -o filesystem "$<"

filesystem/%.font64: assets/%.fnt
	@mkdir -p $(dir $@)
	@echo "    [FONT] $@"
	$(N64_MKFONT) $(MKFONT_FLAGS) -o filesystem "$<"

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) $(MKSPRITE_FLAGS) -o filesystem "$<"

filesystem/Pacifico.font64: MKFONT_FLAGS+=--size 13 --outline 1 --no-kerning
filesystem/Roboto-Medium.font64: MKFONT_FLAGS=--size 12 --range 20-7F --range 80-52F --outline 1
filesystem/roundabout.font64: MKFONT_FLAGS+=--size 14
filesystem/timesplitter.font64: MKFONT_FLAGS=--size 20
filesystem/EnterCommand.font64: MKFONT_FLAGS+=--outline 1
filesystem/m5x7.font64: MKFONT_FLAGS+=--outline 1
filesystem/xolonium.font64: MKFONT_FLAGS=--range 20-7F --range 80-52F
filesystem/CyborgSister.font64: MKFONT_FLAGS+=--outline 1
filesystem/Lato.font64: MKFONT_FLAGS=--range 20-7F --range 80-52F --outline 1
filesystem/droid-sans.font64: MKFONT_FLAGS=--range 20-7F --range 80-52F
filesystem/montreal.font64: MKFONT_FLAGS=--range 20-7F --range 80-52F
filesystem/Liver.font64: MKFONT_FLAGS+=--size 16
filesystem/monogram.font64: MKFONT_FLAGS+=--outline 1
filesystem/at01.font64: MKFONT_FLAGS+=--outline 1
filesystem/abaddon.font64: MKFONT_FLAGS+=--outline 1
filesystem/bitpotion.font64: MKFONT_FLAGS+=--outline 1
filesystem/minimalpixel.font64: MKFONT_FLAGS+=--size 7
filesystem/slab.font64: MKFONT_FLAGS+=--monochrome --size 16
filesystem/chunkysans.font64: MKFONT_FLAGS+=--size 6
filesystem/avenuepixel.font64: MKFONT_FLAGS+=--size 20 --monochrome --outline 1
filesystem/raccoon.font64: MKFONT_FLAGS+=--outline 1
filesystem/Silver.font64: MKFONT_FLAGS=--outline 1 --size 20 --monochrome --range 20-7F --range 80-52F
filesystem/FiraMonoBold.font64: MKFONT_FLAGS=--size 12 --outline 1 --range 20-7F --range 80-1FF -r 2026-2026 --ellipsis 2026,1

$(BUILD_DIR)/fontgallery.dfs: $(assets_conv) 
$(BUILD_DIR)/fontgallery.elf: $(src:%.c=$(BUILD_DIR)/%.o)

fontgallery.z64: N64_ROM_TITLE="RDPQ Font Demo"
fontgallery.z64: $(BUILD_DIR)/fontgallery.dfs 

clean:
	rm -rf $(BUILD_DIR) filesystem fontgallery.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
