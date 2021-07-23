PROG_NAME = ucodetest

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
N64TOOLFLAGS = -l 1M -h $(ROOTDIR)/mips64-elf/lib/header -t "ucode Test"

ifeq ($(N64_BYTE_SWAP),true)
$(PROG_NAME).v64: $(PROG_NAME).z64
	dd conv=swab if=$^ of=$@
endif

$(PROG_NAME).z64: $(PROG_NAME).bin
	$(N64TOOL) $(N64TOOLFLAGS) -o $@ $^
	$(CHKSUM64) $@

$(PROG_NAME).bin: $(PROG_NAME).elf
	$(OBJCOPY) $< $@ -O binary

# Link the program and basic-loader together into an ELF executable
$(PROG_NAME).elf : $(PROG_NAME).o basic-loader.o
	$(LD) -o $@ $^ $(LDFLAGS)

# Compile basic-loader.S with the text and data sections from basic.S
basic-loader.o: basic-loader.S basic-text.section.bin basic-data.section.bin
	$(CC) -c -o $@ $<

# Compile basic.S and extract the text and data sections
basic.o basic-text.section.bin basic-data.section.bin: basic.S
	$(CC) -c -o basic.o $^
	$(OBJCOPY) \
		--dump-section .text=basic-text.section.bin \
		--dump-section .data=basic-data.section.bin \
		basic.o

.PHONY: clean
clean:
	rm -f *.v64 *.z64 *.bin *.elf *.o 
