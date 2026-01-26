BUILD_DIR=build
include $(N64_INST)/include/n64.mk

src = meshviewer.c
assets_glb = $(wildcard assets/*.glb)
assets_models = $(addprefix filesystem/models/,$(notdir $(assets_glb:%.glb=%.model64)))

all: meshviewer.z64

filesystem/models/%.model64: assets/%.glb
	@mkdir -p $(dir $@)
	@echo "    [MODEL] $@"
	@$(N64_MKMODEL) --anim-no-stream --compress 0 -o "$(dir $@)" "$<"

$(BUILD_DIR)/meshviewer.dfs: $(assets_models)
$(BUILD_DIR)/meshviewer.elf: $(src:%.c=$(BUILD_DIR)/%.o)

meshviewer.z64: N64_ROM_TITLE="Mesh Viewer"
meshviewer.z64: $(BUILD_DIR)/meshviewer.dfs

clean:
	rm -rf $(BUILD_DIR) filesystem/ meshviewer.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
