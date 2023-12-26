BUILD_DIR=build
include $(N64_INST)/include/n64.mk

src = compression.c
assets = $(wildcard assets/*)

assets_conv = $(addprefix filesystem/,$(notdir $(assets:%=%.c0))) \
			  $(addprefix filesystem/,$(notdir $(assets:%=%.c1))) \
              $(addprefix filesystem/,$(notdir $(assets:%=%.c2))) \
              $(addprefix filesystem/,$(notdir $(assets:%=%.c3)))

AUDIOCONV_FLAGS ?=
MKSPRITE_FLAGS ?=

all: compression.z64

filesystem/%.c0: assets/%
	@mkdir -p $(dir $@)/c0
	@echo "    [MKASSET] $@"
	$(N64_BINDIR)/mkasset -c 0 -w 256 -o filesystem/c0 $<
	mv "filesystem/c0/$(notdir $<)" "$@"

filesystem/%.c1: assets/%	
	@mkdir -p $(dir $@)/c1
	@echo "    [MKASSET] $@"
	$(N64_BINDIR)/mkasset -c 1 -w 256 -o filesystem/c1 $<
	mv "filesystem/c1/$(notdir $<)" "$@"

filesystem/%.c2: assets/%
	@mkdir -p $(dir $@)/c2
	@echo "    [MKASSET] $@"
	$(N64_BINDIR)/mkasset -c 2 -w 256 -o filesystem/c2 $<
	mv "filesystem/c2/$(notdir $<)" "$@"

filesystem/%.c3: assets/%
	@mkdir -p $(dir $@)/c3
	@echo "    [MKASSET] $@"
	$(N64_BINDIR)/mkasset -c 3 -w 256 -o filesystem/c3 $<
	mv "filesystem/c3/$(notdir $<)" "$@"

$(BUILD_DIR)/compression.dfs: $(assets_conv)
$(BUILD_DIR)/compression.elf: $(src:%.c=$(BUILD_DIR)/%.o)

compression.z64: N64_ROM_TITLE="Compression Bench"
compression.z64: $(BUILD_DIR)/compression.dfs 

clean:
	rm -rf $(BUILD_DIR) compression.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
