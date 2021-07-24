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
N64TOOL = $(SDK_DIR)/bin/n64tool
ED64ROMCONFIG = $(SDK_DIR)/bin/ed64romconfig
CHKSUM64 = $(SDK_DIR)/bin/chksum64

# LibDragon Flags
N64TOOLFLAGS = --size 1M --header $(SDK_LIB_DIR)/header --title $(PROG_TITLE)
ED64ROMCONFIGFLAGS = --rtc --savetype none

# Optionally byte-swap the ROM Image
ifeq ($(N64_BYTE_SWAP),true)
$(PROG_NAME).v64: $(PROG_NAME).z64
	dd conv=swab if=$^ of=$@
endif

# Final ROM Image
$(PROG_NAME).z64: $(PROG_NAME).bin
	$(N64TOOL) -o $@ $(N64TOOLFLAGS) $<
	$(ED64ROMCONFIG) $(ED64ROMCONFIGFLAGS) $@
	$(CHKSUM64) $@

# Raw stripped binary executable
$(PROG_NAME).bin: $(PROG_NAME).elf
	$(OBJCOPY) -O binary $< $@

# Linked object code executable
$(PROG_NAME).elf: $(PROG_NAME).o
	$(LD) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f *.v64 *.z64 *.bin *.elf *.o
