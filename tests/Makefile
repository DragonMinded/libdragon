PROG_NAME = testrom

ROOTDIR = $(N64_INST)
GCCN64PREFIX = $(ROOTDIR)/bin/mips64-elf-

CC = $(GCCN64PREFIX)gcc
AS = $(GCCN64PREFIX)as
LD = $(GCCN64PREFIX)ld
OBJCOPY = $(GCCN64PREFIX)objcopy
N64TOOL = $(ROOTDIR)/bin/n64tool
CHKSUM64 = $(ROOTDIR)/bin/chksum64
MKDFS = $(ROOTDIR)/bin/mkdfs

ASFLAGS = -mtune=vr4300 -march=vr4300
CFLAGS = -std=gnu99 -march=vr4300 -mtune=vr4300 -O2 -Wall -Werror -I$(ROOTDIR)/mips64-elf/include -I$(ROOTDIR)/include
LDFLAGS = -L$(ROOTDIR)/mips64-elf/lib -L$(ROOTDIR)/lib -ldragon -lmikmod -lc -lm -ldragonsys -Tn64.ld  --gc-sections
N64TOOLFLAGS = -l 2M -h $(ROOTDIR)/mips64-elf/lib/header -t "libdragon tests"

ifeq ($(N64_BYTE_SWAP),true)
$(PROG_NAME).v64: $(PROG_NAME).z64
	dd conv=swab if=$^ of=$@
endif

CFLAGS+=-MMD    # automatic .d dependency generation

all: $(PROG_NAME).z64 $(PROG_NAME)_emu.z64

$(PROG_NAME).z64: $(PROG_NAME).bin $(PROG_NAME).dfs
	$(N64TOOL) $(N64TOOLFLAGS) -o $@ $(PROG_NAME).bin -s 1M $(PROG_NAME).dfs
	$(CHKSUM64) $@
$(PROG_NAME)_emu.z64: $(PROG_NAME)_emu.bin $(PROG_NAME).dfs
	$(N64TOOL) $(N64TOOLFLAGS) -o $@ $(PROG_NAME)_emu.bin -s 1M $(PROG_NAME).dfs
	$(CHKSUM64) $@

$(PROG_NAME).bin: $(PROG_NAME).elf
	$(OBJCOPY) $< $@ -O binary
$(PROG_NAME)_emu.bin: $(PROG_NAME)_emu.elf
	$(OBJCOPY) $< $@ -O binary

$(PROG_NAME).elf: $(PROG_NAME).o
	$(LD) -o $@ $^ $(LDFLAGS)
$(PROG_NAME)_emu.elf: $(PROG_NAME)_emu.o
	$(LD) -o $@ $^ $(LDFLAGS)

$(PROG_NAME)_emu.o: $(PROG_NAME).c
	$(CC) $(CFLAGS) -DIN_EMULATOR=1 -c -o $@ $<

$(PROG_NAME).dfs:
	$(MKDFS) $@ ./filesystem/

clean:
	rm -f *.v64 *.z64 *.elf *.o *.bin *.dfs *.d

-include $(PROG_NAME).d $(PROG_NAME)_emu.d

