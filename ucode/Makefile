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
	$(CC) -c -o $@ $^

basic-clean:
	rm -f *.o *.bin

.PHONY : basic basic-clean