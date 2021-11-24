BUILD_DIR=build
include $(N64_INST)/include/n64.mk

src = dldemo.c
assets_xm = $(wildcard assets/*.xm)
assets_wav = $(wildcard assets/*.wav)

assets_conv = $(addprefix filesystem/,$(notdir $(assets_xm:%.xm=%.xm64))) \
              $(addprefix filesystem/,$(notdir $(assets_wav:%.wav=%.wav64)))

AUDIOCONV_FLAGS ?=

all: dldemo.z64

filesystem/%.xm64: assets/%.xm
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	@$(N64_AUDIOCONV) $(AUDIOCONV_FLAGS) -o filesystem $<

filesystem/%.wav64: assets/%.wav
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	@$(N64_AUDIOCONV) -o filesystem $<

$(BUILD_DIR)/dldemo.dfs: $(assets_conv)
$(BUILD_DIR)/dldemo.elf: $(src:%.c=$(BUILD_DIR)/%.o)

dldemo.z64: N64_ROM_TITLE="DL Demo"
dldemo.z64: $(BUILD_DIR)/dldemo.dfs 

clean:
	rm -rf $(BUILD_DIR) dldemo.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
