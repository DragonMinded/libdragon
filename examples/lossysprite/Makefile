BUILD_DIR=build
include $(N64_INST)/include/n64.mk

src = lossysprite.c

BG_INDICES = 0 1 2 3 4 5 6
RAW_SPRITES  = $(foreach i,$(BG_INDICES),filesystem/bg$(i)_raw.sprite)
BCSP_SPRITES = $(foreach i,$(BG_INDICES),filesystem/bg$(i)_bcsp.sprite)
LSPR_SPRITES = $(foreach i,$(BG_INDICES),filesystem/bg$(i)_lspr.sprite)

assets_conv = \
	$(RAW_SPRITES) \
	$(BCSP_SPRITES) \
	$(LSPR_SPRITES) \
	filesystem/n64brew.sprite

all: lossysprite.z64

# Each bgN.png is encoded three ways so the demo can switch formats at runtime.
# mksprite -o takes a directory and names the output after the input file, so
# each rule uses a per-format scratch dir to avoid racing on bg$*.sprite when
# make runs the rules in parallel, then renames into filesystem/.
filesystem/bg%_raw.sprite: assets/bg%.png
	@mkdir -p $(dir $@) $(BUILD_DIR)/sprites_raw
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) --format RGBA16 --compress 1 -o $(BUILD_DIR)/sprites_raw "$<"
	mv $(BUILD_DIR)/sprites_raw/bg$*.sprite "$@"

filesystem/bg%_bcsp.sprite: assets/bg%.png
	@mkdir -p $(dir $@) $(BUILD_DIR)/sprites_bcsp
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) --format RGBA16 --lossy 75 --compress 1 -o $(BUILD_DIR)/sprites_bcsp "$<"
	mv $(BUILD_DIR)/sprites_bcsp/bg$*.sprite "$@"

filesystem/bg%_lspr.sprite: assets/bg%.png
	@mkdir -p $(dir $@) $(BUILD_DIR)/sprites_lspr
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) --format RGBA16 --lossy 75 --compress 3 -o $(BUILD_DIR)/sprites_lspr "$<"
	mv $(BUILD_DIR)/sprites_lspr/bg$*.sprite "$@"

# Brew sprite in standard RGBA16 format (losslessly compressed)
filesystem/n64brew.sprite: assets/n64brew.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) --format RGBA16 --compress -o filesystem "$<"

$(BUILD_DIR)/lossysprite.dfs: $(assets_conv)
$(BUILD_DIR)/lossysprite.elf: $(src:%.c=$(BUILD_DIR)/%.o)

lossysprite.z64: N64_ROM_TITLE="Lossy Sprite Demo"
lossysprite.z64: $(BUILD_DIR)/lossysprite.dfs

clean:
	rm -rf $(BUILD_DIR) filesystem lossysprite.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
