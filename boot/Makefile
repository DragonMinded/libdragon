ifneq ($(V),1)
.SILENT:
endif

SOURCE_DIR=.

ifeq ($(shell uname -s),Darwin)
	SED=gsed
else
	SED=sed
endif

N64_GCCPREFIX ?= $(N64_INST)
N64_GCCPREFIX_TRIPLET = $(N64_GCCPREFIX)/bin/mips64-elf-
N64_CC = $(N64_GCCPREFIX_TRIPLET)gcc
N64_AS = $(N64_GCCPREFIX_TRIPLET)as
N64_LD = $(N64_GCCPREFIX_TRIPLET)ld
N64_OBJCOPY = $(N64_GCCPREFIX_TRIPLET)objcopy
N64_OBJDUMP = $(N64_GCCPREFIX_TRIPLET)objdump
N64_SIZE = $(N64_GCCPREFIX_TRIPLET)size
N64_READELF = $(N64_GCCPREFIX_TRIPLET)readelf

N64_ROOTDIR = $(N64_INST)
N64_INCLUDEDIR = $(N64_ROOTDIR)/mips64-elf/include
N64_LIBDIR = $(N64_ROOTDIR)/mips64-elf/lib

N64_CFLAGS =  -march=vr4300 -mtune=vr4300 -MMD
N64_CFLAGS += -DN64 -Os -Wall -Werror -Wno-error=deprecated-declarations -fdiagnostics-color=always
N64_CFLAGS += -Wno-error=unused-function -ffreestanding -nostdlib -ffunction-sections
N64_CFLAGS += -G0 # gp is not initialized (don't use it)
N64_CFLAGS += -mabi=32 -mgp32 -mfp32 -msingle-float # Can't compile for 64bit ABI because DMEM/IMEM don't support 64-bit access
#N64_CFLAGS += -flto

N64_ASFLAGS = -mtune=vr4300 -march=vr4300 -Wa,--fatal-warnings
N64_ASFLAGS =  -mabi=32 -mgp32 -mfp32 -msingle-float -G0
N64_RSPASFLAGS = -march=mips1 -mabi=32 -Wa,--fatal-warnings
N64_LDFLAGS = -Wl,-T$(IPL3_LDSCRIPT) -Wl,-Map=build/ipl3.map -Wl,--gc-sections

OBJS = build/ipl3.o build/minidragon.o build/rdram.o build/ique_trampoline.o build/entropy.o

ifeq ($(PROD),1)
OBJS += build/loader.o 
N64_CFLAGS += -DNDEBUG
IPL3_ROM=bin/ipl3_prod.z64
IPL3_LDSCRIPT=ipl3_prod.ld
else ifeq ($(COMPAT),1)
N64_CFLAGS += -DNDEBUG -DCOMPAT
OBJS += build/loader_compat.o 
IPL3_ROM=bin/ipl3_compat.z64
IPL3_LDSCRIPT=ipl3_compat.ld
else
OBJS += build/loader.o build/debug.o build/boot_trampoline.o 
IPL3_ROM=bin/ipl3_dev.z64
IPL3_LDSCRIPT=ipl3_dev.ld
endif

all: $(IPL3_ROM)

build/%.o: %.c
	@echo "    [CC] $@"
	@mkdir -p build
# Compile relocatable code. We need this for stage 2 which is relocated to RDRAM,
# but in general it won't hurt for boot code. GCC MIPS doesn't have a way to do
# this simple transformation so we do it ourselves by replacing jal/j with bal/b
# on the assembly output of the compiler.
	$(N64_CC) -S -c $(N64_CFLAGS) -o $@.s $<
	$(SED) -i 's/\bjal\b/bal/g' $@.s
	$(SED) -i 's/\bj\b/b/g' $@.s
	$(N64_AS) $(N64_ASFLAGS) -o $@ $@.s

build/%.o: %.S
	@echo "    [AS] $@"
	@mkdir -p build
	$(N64_CC) -c $(N64_ASFLAGS) -o $@ $<

%.z64: build/ipl3.elf
	@echo "    [Z64] $@"
	$(N64_OBJCOPY) -O binary $< $@

build/ipl3.elf: $(IPL3_LDSCRIPT) $(OBJS)
	@echo "    [LD] $@"
	$(N64_CC) $(N64_CFLAGS) $(N64_LDFLAGS) -o $@ $(filter %.o,$^)
	$(N64_READELF) --wide --sections $@ | grep .text

install: $(IPL3_ROM)
	xxd -i -n default_ipl3 $< >../tools/ipl3.h

run: ipl3.z64
	g64drive upload --autocic=false ipl3.z64 && g64drive cic 6102 && g64drive debug

disasm: build/ipl3.elf
	$(N64_OBJDUMP) -D build/ipl3.elf

clean:
	rm -rf build bin/ipl3_dev.z64 bin/ipl3_prod.z64

-include $(wildcard build/*.d)

.PHONY: all disasm run
