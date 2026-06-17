ROMNAME := helloworld

BUILD_DIR = build

ifeq ($(N64_INST),)
  $(error N64_INST is not set)
endif
$(info Using N64_INST=$(N64_INST))
ifeq ($(wildcard $(N64_INST)/include/n64.mk),)
  $(error $(N64_INST)/include/n64.mk does not exist: N64_INST may be incorrect)
endif
# Including n64.mk is of primary importance. It essentially contains the build system.
# Do not hand-roll your own make rules as they may break with libdragon updates.
include $(N64_INST)/include/n64.mk

all: $(ROMNAME).z64
.PHONY: all

# C_FILES lists all .c files under src/
C_FILES := $(shell find src -name '*.c')
OBJS := $(addprefix $(BUILD_DIR)/,$(C_FILES:.c=.o))

$(BUILD_DIR)/$(ROMNAME).elf: $(OBJS)

$(ROMNAME).z64: N64_ROM_TITLE = "Hello World"

# Use `make clean` to delete build artifacts (build folder and rom)
clean:
	$(RM) -r $(BUILD_DIR) *.z64
.PHONY: clean

ifneq ($(wildcard $(BUILD_DIR)),)
  -include $(shell find $(BUILD_DIR) -name '*.d')
endif
