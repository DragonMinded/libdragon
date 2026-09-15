BUILD_DIR=build
# This example uses BC1Q and H264I, which are preview APIs
LIBDRAGON_PREVIEW = 2
include $(N64_INST)/include/n64.mk

src = lossysprite.c

BG_INDICES = 0 1 2 3 4 5 6
RAW_SPRITES   = $(foreach i,$(BG_INDICES),filesystem/bg$(i)_raw.sprite)
BC1Q_SPRITES  = $(foreach i,$(BG_INDICES),filesystem/bg$(i)_bc1q.sprite)
H264I_SPRITES = $(foreach i,$(BG_INDICES),filesystem/bg$(i)_h264i.sprite)

assets_conv = \
	$(RAW_SPRITES) \
	$(BC1Q_SPRITES) \
	$(H264I_SPRITES) \
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

filesystem/bg%_bc1q.sprite: assets/bg%.png
	@mkdir -p $(dir $@) $(BUILD_DIR)/sprites_bc1q
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) --format RGBA16 --lossy 75 --compress 1 -o $(BUILD_DIR)/sprites_bc1q "$<"
	mv $(BUILD_DIR)/sprites_bc1q/bg$*.sprite "$@"

filesystem/bg%_h264i.sprite: assets/bg%.png
	@mkdir -p $(dir $@) $(BUILD_DIR)/sprites_h264i
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) --format RGBA16 --lossy 75 --compress 3 -o $(BUILD_DIR)/sprites_h264i "$<"
	mv $(BUILD_DIR)/sprites_h264i/bg$*.sprite "$@"

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
