ROOTDIR = $(N64_INST)
GCCN64PREFIX = $(ROOTDIR)/bin/mips64-elf-
CHKSUM64PATH = $(ROOTDIR)/bin/chksum64
MKDFSPATH = $(ROOTDIR)/bin/mkdfs
HEADERPATH = $(ROOTDIR)/mips64-elf/lib
N64TOOL = $(ROOTDIR)/bin/n64tool
HEADERNAME = header
LINK_FLAGS = -L$(ROOTDIR)/mips64-elf/lib -ldragon -lc -lm -ldragonsys -Tn64.ld --gc-sections
CFLAGS = -std=gnu99 -march=vr4300 -mtune=vr4300 -O2 -Wall -Werror -I$(ROOTDIR)/mips64-elf/include
ASFLAGS = -mtune=vr4300 -march=vr4300
CC = $(GCCN64PREFIX)gcc
AS = $(GCCN64PREFIX)as
LD = $(GCCN64PREFIX)ld
OBJCOPY = $(GCCN64PREFIX)objcopy

ifeq ($(N64_BYTE_SWAP),true)
ROM_EXTENSION = .v64
N64_FLAGS = -b -l 8M -h $(HEADERPATH)/$(HEADERNAME)
else
ROM_EXTENSION = .z64
N64_FLAGS = -l 8M -h $(HEADERPATH)/$(HEADERNAME)
endif

CFLAGS+=-MMD    # automatic .d dependency generation

all: testrom$(ROM_EXTENSION) testrom_emu$(ROM_EXTENSION)

testrom$(ROM_EXTENSION): testrom.elf test.dfs
	$(OBJCOPY) testrom.elf testrom.bin -O binary
	rm -f testrom$(ROM_EXTENSION)
	$(N64TOOL) $(N64_FLAGS) -t "libdragon tests" -o testrom$(ROM_EXTENSION) testrom.bin -s 1M test.dfs
	$(CHKSUM64PATH) testrom$(ROM_EXTENSION)

testrom_emu$(ROM_EXTENSION): testrom_emu.elf test.dfs
	$(OBJCOPY) testrom_emu.elf testrom_emu.bin -O binary
	rm -f testrom_emu$(ROM_EXTENSION)
	$(N64TOOL) $(N64_FLAGS) -t "libdragon tests" -o testrom_emu$(ROM_EXTENSION) testrom_emu.bin -s 1M test.dfs
	$(CHKSUM64PATH) testrom_emu$(ROM_EXTENSION)

testrom_emu.o : testrom.c
	$(CC) $(CFLAGS) -DIN_EMULATOR=1 -c -o $@ $<

testrom.elf : testrom.o
	$(LD) -o testrom.elf testrom.o $(LINK_FLAGS)
testrom_emu.elf : testrom_emu.o
	$(LD) -o testrom_emu.elf testrom_emu.o $(LINK_FLAGS)

test.dfs:
	$(MKDFSPATH) test.dfs ./filesystem/

clean:
	rm -f *.v64 *.z64 *.elf *.o *.bin *.dfs *.d

-include testrom.d testrom_emu.d

