ASFLAGS = -mtune=vr4300 -march=vr4300
CFLAGS = -std=gnu99 -O2 -G0 -Wall -Werror -mtune=vr4300 -march=vr4300
LINK_FLAGS = -L$(ROOTDIR)/mips64-elf/lib -ldragon -lc -lm -ldragonsys -Tn64ld.x
N64PREFIX = $(N64_INST)/bin/mips64-elf-
CC = $(N64PREFIX)gcc
AS = $(N64PREFIX)as
LD = $(N64PREFIX)ld
AR = $(N64PREFIX)ar
OBJCOPY = $(N64PREFIX)objcopy

all: basic
clean: basic-clean

basic: basic.o

basic.bin: basic.S
	$(CC) -c -o tmp.o basic.S
	$(OBJCOPY) -O binary -j .text tmp.o basic.bin

basic.o: basic.bin
	$(OBJCOPY) -I binary -O elf32-bigmips -B mips4300 --redefine-sym _binary_basic_bin_start=basic_ucode_start --redefine-sym _binary_basic_bin_end=basic_ucode_end --redefine-sym _binary_basic_bin_size=basic_ucode_size --rename-section .text=.data  basic.bin basic.o

basic-clean:
	rm -f *.o *.bin

.PHONY : basic basic-clean