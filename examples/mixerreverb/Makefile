BUILD_DIR=build
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/mixerreverb.o

assets = $(wildcard assets/*.wav)
assets_conv = $(addprefix filesystem/,$(notdir $(assets:%.wav=%.wav64)))

all: mixerreverb.z64

filesystem/%.wav64: assets/%.wav
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	@$(N64_AUDIOCONV) --wav-compress 3 -o filesystem $<

$(BUILD_DIR)/mixerreverb.dfs: $(assets_conv)
$(BUILD_DIR)/mixerreverb.elf: $(OBJS)

mixerreverb.z64: N64_ROM_TITLE="Mixer Reverb"
mixerreverb.z64: $(BUILD_DIR)/mixerreverb.dfs

clean:
	rm -f $(BUILD_DIR)/* mixerreverb.z64 $(assets_conv)

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
