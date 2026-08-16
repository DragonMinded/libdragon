# menudemo - A simple menu demo for the N64
#
# This file builds the menudemo ROM for the N64.
#
# This is free and unencumbered software released into the public domain.
#
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
#
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#
# For more information, please refer to <https://unlicense.org>
#

SOURCE_DIR=src
BUILD_DIR=build
include $(N64_INST)/include/n64.mk

all: menudemo.z64
.PHONY: all

ASSETS_DIR = assets
FILESYSTEM_DIR = filesystem

assets = $(ASSETS_DIR)/background.png $(ASSETS_DIR)/logo.ci4.png
assets += $(ASSETS_DIR)/bop.wav64 $(ASSETS_DIR)/bap.wav64
assets += $(ASSETS_DIR)/miafan2010_-_you_would_be_here.xm64

assets_conv = $(addprefix $(FILESYSTEM_DIR)/,$(notdir $(assets:%.png=%.sprite)))

WAV64_AUDIOCONV_FLAGS ?= --wav-compress 1,bits=2
XM64_AUDIOCONV_FLAGS ?= --xm-compress-data 3 --xm-8bit
MKSPRITE_FLAGS ?=

OBJS = $(BUILD_DIR)/menudemo.o

$(FILESYSTEM_DIR)/%.sprite: $(ASSETS_DIR)/%.png
	@mkdir -p $(FILESYSTEM_DIR)
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	@$(N64_MKSPRITE) $(MKSPRITE_FLAGS) -o $(FILESYSTEM_DIR) "$<"

$(FILESYSTEM_DIR)/%.wav64: $(ASSETS_DIR)/%.wav
	@mkdir -p $(dir $@)
	@echo "    [AUDIOCONV] $@"
	@$(N64_AUDIOCONV) $(WAV64_AUDIOCONV_FLAGS) -o $(FILESYSTEM_DIR) "$<"

$(FILESYSTEM_DIR)/%.xm64: $(ASSETS_DIR)/%.xm
	@mkdir -p $(dir $@)
	@echo "    [AUDIOCONV] $@"
	@$(N64_AUDIOCONV) $(XM64_AUDIOCONV_FLAGS) -o $(FILESYSTEM_DIR) "$<"


menudemo.z64: N64_ROM_TITLE="menudemo"
menudemo.z64: $(BUILD_DIR)/menudemo.dfs

$(BUILD_DIR)/menudemo.elf: $(OBJS)
$(BUILD_DIR)/menudemo.dfs: $(assets_conv)
	@echo "	[DFS] $@"
	if [ ! -s "$<"]; then rm -f "$<"; fi
	$(N64_MKDFS) "$@" $(FILESYSTEM_DIR) >/dev/null

clean:
	rm -f $(BUILD_DIR)/* *.z64
	rm -rf $(BUILD_DIR)
	rm -rf $(FILESYSTEM_DIR)/*
	rm -rf $(FILESYSTEM_DIR)
.PHONY: clean

-include $(wildcard $(BUILD_DIR)/*.d)