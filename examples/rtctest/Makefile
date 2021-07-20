PROG_NAME = rtctest
PROG_TITLE = "RTC Test"

# Paths
SDK_DIR = $(N64_INST)
SDK_LIB_DIR = $(SDK_DIR)/mips64-elf/lib
N64_GCC_PREFIX = $(SDK_DIR)/bin/mips64-elf-

# GCC binaries
CC = $(N64_GCC_PREFIX)gcc
CXX = $(N64_GCC_PREFIX)g++
AS = $(N64_GCC_PREFIX)as
LD = $(N64_GCC_PREFIX)ld
OBJCOPY = $(N64_GCC_PREFIX)objcopy

# GCC Flags
CFLAGS = -std=gnu99 -march=vr4300 -mtune=vr4300 -g -Og -Wall -Werror
CFLAGS += -I$(SDK_DIR)/mips64-elf/include
LDFLAGS = --library=dragon --library=c --library=m --library=dragonsys
LDFLAGS += --library-path=$(SDK_LIB_DIR) --script=n64.ld --gc-sections

# LibDragon binaries
ROM_HEADER = $(SDK_LIB_DIR)/header
CHKSUM64 = $(SDK_DIR)/bin/chksum64
N64TOOL = $(SDK_DIR)/bin/n64tool

# LibDragon Flags
OUT_SIZE = 1052672B # 52672B HEADER + 1M (minimum) ROM
N64TOOLFLAGS = -l $(OUT_SIZE) -h $(ROM_HEADER) -t $(PROG_TITLE)

# Byte-swap ROM
ifeq ($(N64_BYTE_SWAP),true)
ROM_EXTENSION = .v64
N64TOOLFLAGS += -b
else
ROM_EXTENSION = .z64
endif

# Build products
OBJS := $(PROG_NAME).o
DEPS := $(OBJS:.o=.d)
LINKED_ELF = $(PROG_NAME).elf
RAW_BINARY = $(PROG_NAME).bin
ROM_FILE = $(PROG_NAME)$(ROM_EXTENSION)

# ROM Image
$(ROM_FILE): $(RAW_BINARY)
	@rm -f $@
	$(N64TOOL) -o $@ $(N64TOOLFLAGS) $<
	$(CHKSUM64) $@

# Raw stripped binary
$(RAW_BINARY): $(LINKED_ELF)
	$(OBJCOPY) -O binary $< $@

# Linked object code binary
$(LINKED_ELF): $(OBJS)
	$(LD) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f *.v64 *.z64 *.elf *.bin *.o *.d
