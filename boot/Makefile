SOURCE_DIR=.

N64_GCCPREFIX ?= $(N64_INST)
N64_GCCPREFIX_TRIPLET = $(N64_GCCPREFIX)/bin/mips64-elf-
N64_CC = $(N64_GCCPREFIX_TRIPLET)gcc
N64_LD = $(N64_GCCPREFIX_TRIPLET)ld
N64_OBJCOPY = $(N64_GCCPREFIX_TRIPLET)objcopy
N64_OBJDUMP = $(N64_GCCPREFIX_TRIPLET)objdump
N64_SIZE = $(N64_GCCPREFIX_TRIPLET)size

N64_ROOTDIR = $(N64_INST)
N64_INCLUDEDIR = $(N64_ROOTDIR)/mips64-elf/include
N64_LIBDIR = $(N64_ROOTDIR)/mips64-elf/lib

N64_CFLAGS =  -march=vr4300 -mtune=vr4300 -MMD
N64_CFLAGS += -DN64 -O2 -Wall -Werror -Wno-error=deprecated-declarations -fdiagnostics-color=always
N64_CFLAGS += -Wno-error=unused-function -ffreestanding -nostdlib -ffunction-sections
N64_CFLAGS += -G0 # gp is not initialized (don't use it)
N64_CFLAGS += -mabi=32  # Can't compile for 64bit ABI because DMEM/IMEM don't support 64-bit access
#N64_CFLAGS += -flto

N64_ASFLAGS = -mtune=vr4300 -march=vr4300 -Wa,--fatal-warnings
N64_RSPASFLAGS = -march=mips1 -mabi=32 -Wa,--fatal-warnings
N64_LDFLAGS = -Wl,-Tipl3.ld -Wl,-Map=build/ipl3.map

all: ipl3_dev.z64 ipl3_unsigned.z64

build/%.o: %.c
	mkdir -p build
	$(N64_CC) -c $(N64_CFLAGS) -o $@ $<

build/ipl3.bin: ipl3.ld build/ipl3.o build/minidragon.o build/rdram.o build/loader.o build/debug.o
	$(N64_CC) $(N64_CFLAGS) $(N64_LDFLAGS) -o build/ipl3.elf $(filter %.o,$^)
	$(N64_SIZE) -G build/ipl3.elf
	$(N64_OBJCOPY) -O binary build/ipl3.elf build/ipl3.bin

ipl3_dev.z64: build/ipl3.bin boot_signed.z64
	cat boot_signed.z64 build/ipl3.bin >ipl3_dev.z64

ipl3_unsigned.z64: build/ipl3.bin
	cp build/ipl3.bin ipl3_unsigned.z64

install-dev: ipl3_dev.z64
	xxd -i -n default_ipl3 ipl3_dev.z64 >../tools/ipl3.h

run: ipl3.z64
	g64drive upload --autocic=false ipl3.z64 && g64drive cic 6102 && g64drive debug

disasm: build/ipl3.bin
	$(N64_OBJDUMP) -D build/ipl3.elf

log:
	g64drive download -b rom -o 0x3000000 -s 2048 log.txt && cat log.txt

-include $(wildcard build/*.d)

.PHONY: all disasm run

