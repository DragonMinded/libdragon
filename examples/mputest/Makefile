PROG_NAME = mputest

ROOTDIR = $(N64_INST)
GCCN64PREFIX = $(ROOTDIR)/bin/mips64-elf-

CC = $(GCCN64PREFIX)gcc
AS = $(GCCN64PREFIX)as
LD = $(GCCN64PREFIX)ld
OBJCOPY = $(GCCN64PREFIX)objcopy
N64TOOL = $(ROOTDIR)/bin/n64tool
CHKSUM64 = $(ROOTDIR)/bin/chksum64

ASFLAGS = -mtune=vr4300 -march=vr4300
CFLAGS = -std=gnu99 -march=vr4300 -mtune=vr4300 -O2 -Wall -Werror -I$(ROOTDIR)/mips64-elf/include
LDFLAGS = -L$(ROOTDIR)/mips64-elf/lib -ldragon -lc -lm -ldragonsys -Tn64.ld --gc-sections
N64TOOLFLAGS = -l 1M -h $(ROOTDIR)/mips64-elf/lib/header -t "Mempak Unit Test"

ifeq ($(N64_BYTE_SWAP),true)
$(PROG_NAME).v64: $(PROG_NAME).z64
	dd conv=swab if=$^ of=$@
endif

$(PROG_NAME).z64: $(PROG_NAME).bin
	$(N64TOOL) $(N64TOOLFLAGS) -o $@ $^
	$(CHKSUM64) $@

$(PROG_NAME).bin: $(PROG_NAME).elf
	$(OBJCOPY) $< $@ -O binary

$(PROG_NAME).elf: $(PROG_NAME).o
	$(LD) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f *.v64 *.z64 *.bin *.elf *.o
