BUILD_DIR=build
include $(N64_INST)/include/n64.mk

# Main Executable Source File Lists
main_SRC = ovldemo.c

# Specify Source Files per Overlay
overlay1_SRC = overlay1.c
overlay2_SRC = overlay2.c

#Main ROM Build Rule
all: ovldemo.z64

# File for DSO External Symbols List
# This file is used to make sure symbols used only in DSOs
# exist in the main executable
MAIN_ELF_EXTERNS := $(BUILD_DIR)/ovldemo.externs
# File for Main Executable Symbol Table
MAIN_ELF_SYMBOLS := $(BUILD_DIR)/ovldemo.msym

# Overlay File List
DSO_MODULES = overlay1.dso overlay2.dso
DSO_LIST = $(addprefix filesystem/, $(DSO_MODULES))
# Filesystem Build Rule
$(BUILD_DIR)/ovldemo.dfs: $(DSO_LIST)
# Main ELF Build Rule
# DSO External Symbols List must be specified here
$(BUILD_DIR)/ovldemo.elf: $(main_SRC:%.c=$(BUILD_DIR)/%.o) $(MAIN_ELF_EXTERNS)
# Build DSO External Symbol List
$(MAIN_ELF_EXTERNS): $(DSO_LIST)

# Specify object files (.o) that are part of each overlay
filesystem/overlay1.dso: $(overlay1_SRC:%.c=$(BUILD_DIR)/%.o)
filesystem/overlay2.dso: $(overlay2_SRC:%.c=$(BUILD_DIR)/%.o)

ovldemo.z64: N64_ROM_TITLE="Overlay Demo"
# ROM Build rules
# Main Executable Symbol Table is required for overlay linking
ovldemo.z64: $(BUILD_DIR)/ovldemo.dfs $(MAIN_ELF_SYMBOLS)
# Rule for Main Executable Symbol Table
$(MAIN_ELF_SYMBOLS): $(BUILD_DIR)/ovldemo.elf

clean:
	rm -rf $(BUILD_DIR) filesystem $(DSO_LIST) ovldemo.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
