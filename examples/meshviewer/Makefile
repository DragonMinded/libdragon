BUILD_DIR=build
include $(N64_INST)/include/n64.mk

src = meshviewer.c

assets_glb = $(wildcard assets/*.glb)
assets_gltf = $(wildcard assets/*.gltf)
assets_png = $(wildcard assets/*.png)

assets_models_glb = $(addprefix filesystem/models/,$(notdir $(assets_glb:%.glb=%.model64)))
assets_models_gltf = $(addprefix filesystem/models/,$(notdir $(assets_gltf:%.gltf=%.model64)))
assets_models = $(assets_models_glb) $(assets_models_gltf)

assets_anims_glb = $(addprefix filesystem/models/,$(notdir $(assets_glb:%.glb=%.anims)))
assets_anims_gltf = $(addprefix filesystem/models/,$(notdir $(assets_gltf:%.gltf=%.anims)))
assets_anims = $(assets_anims_glb) $(assets_anims_gltf)

assets_sprites = $(addprefix filesystem/models/,$(notdir $(assets_png:assets/%.png=%.sprite)))

all: meshviewer.z64

filesystem/models/%.model64: assets/%.glb
	@mkdir -p $(dir $@)
	@echo "    [MODEL] $@"
	$(N64_MKMODEL) -v --anim-no-stream --compress 0 -o "$(dir $@)" "$<" 2>&1 \
		| sed -n 's/^Converting animation //p' >"$(@:%.model64=%.anims)"; 

filesystem/models/%.model64: assets/%.gltf assets/%.bin
	@mkdir -p $(dir $@)
	@echo "    [MODEL] $@"
	$(N64_MKMODEL) -v --anim-no-stream --compress 0 -o "$(dir $@)" "$<" 2>&1 \
		| sed -n 's/^Converting animation //p' >"$(@:%.model64=%.anims)"; 

filesystem/models/%.anims: filesystem/models/%.model64
	@true

filesystem/models/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	@$(N64_MKSPRITE) -f RGBA16 -o "$(dir $@)" "$<"

$(BUILD_DIR)/meshviewer.dfs: $(assets_models) $(assets_anims) $(assets_sprites)
$(BUILD_DIR)/meshviewer.elf: $(src:%.c=$(BUILD_DIR)/%.o)

meshviewer.z64: N64_ROM_TITLE="Mesh Viewer"
meshviewer.z64: $(BUILD_DIR)/meshviewer.dfs

clean:
	rm -rf $(BUILD_DIR) filesystem/ meshviewer.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
