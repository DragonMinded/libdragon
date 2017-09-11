ROOTDIR = $(N64_INST)
CFLAGS = -std=gnu99 -O2 -G0 -Wall -Werror -mtune=vr4300 -march=vr4300 -I$(CURDIR)/include -I$(ROOTDIR)/mips64-elf/include
ASFLAGS = -mtune=vr4300 -march=vr4300
N64PREFIX = $(N64_INST)/bin/mips64-elf-
INSTALLDIR = $(N64_INST)
CC = $(N64PREFIX)gcc
AS = $(N64PREFIX)as
LD = $(N64PREFIX)ld
AR = $(N64PREFIX)ar

all: libdragon

libdragon: libdragon.a libdragonsys.a libdragonpp.a

include files.in

examples:
	make -C examples
examples-clean:
	make -C examples clean

doxygen: doxygen.conf
	mkdir -p doxygen/
	doxygen doxygen.conf
doxygen-api: doxygen-public.conf
	mkdir -p doxygen/
	doxygen doxygen-public.conf
doxygen-clean:
	rm -rf $(CURDIR)/doxygen

tools:
	+make -C tools
tools-install:
	make -C tools install
tools-clean:
	make -C tools clean

libdragon.a: $(OFILES_LD)
	$(AR) -rcs -o libdragon.a $(OFILES_LD)
libdragonsys.a: $(OFILES_LDS)
	$(AR) -rcs -o libdragonsys.a $(OFILES_LDS)
libdragonpp.a: $(OFILES_LDP)
	$(AR) -rcs -o libdragonpp.a $(OFILES_LDP)

install: libdragon.a libdragonsys.a libdragonpp.a
	install -m 0644 libdragon.a $(INSTALLDIR)/mips64-elf/lib/libdragon.a
	install -m 0644 n64ld.x $(INSTALLDIR)/mips64-elf/lib/n64ld.x
	install -m 0644 n64ld_cpp.x $(INSTALLDIR)/mips64-elf/lib/n64ld_cpp.x
	install -m 0644 n64ld_exp_cpp.x $(INSTALLDIR)/mips64-elf/lib/n64ld_exp_cpp.x
	install -m 0644 header $(INSTALLDIR)/mips64-elf/lib/header
	install -m 0644 libdragonsys.a $(INSTALLDIR)/mips64-elf/lib/libdragonsys.a
	install -m 0644 libdragonpp.a $(INSTALLDIR)/mips64-elf/lib/libdragonpp.a
	install -m 0644 include/n64sys.h $(INSTALLDIR)/mips64-elf/include/n64sys.h
	install -m 0644 include/interrupt.h $(INSTALLDIR)/mips64-elf/include/interrupt.h
	install -m 0644 include/dma.h $(INSTALLDIR)/mips64-elf/include/dma.h
	install -m 0644 include/dragonfs.h $(INSTALLDIR)/mips64-elf/include/dragonfs.h
	install -m 0644 include/audio.h $(INSTALLDIR)/mips64-elf/include/audio.h
	install -m 0644 include/display.h $(INSTALLDIR)/mips64-elf/include/display.h
	install -m 0644 include/console.h $(INSTALLDIR)/mips64-elf/include/console.h
	install -m 0644 include/mempak.h $(INSTALLDIR)/mips64-elf/include/mempak.h
	install -m 0644 include/controller.h $(INSTALLDIR)/mips64-elf/include/controller.h
	install -m 0644 include/graphics.h $(INSTALLDIR)/mips64-elf/include/graphics.h
	install -m 0644 include/rdp.h $(INSTALLDIR)/mips64-elf/include/rdp.h
	install -m 0644 include/timer.h $(INSTALLDIR)/mips64-elf/include/timer.h
	install -m 0644 include/exception.h $(INSTALLDIR)/mips64-elf/include/exception.h
	install -m 0644 include/system.h $(INSTALLDIR)/mips64-elf/include/system.h
	install -m 0644 include/dir.h $(INSTALLDIR)/mips64-elf/include/dir.h
	install -m 0644 include/libdragon.h $(INSTALLDIR)/mips64-elf/include/libdragon.h

clean:
	rm -f *.o *.a
	rm -rf $(CURDIR)/build

clobber: clean doxygen-clean examples-clean tools-clean

.PHONY : clobber clean doxygen-clean doxygen doxygen-api examples examples-clean tools tools-clean tools-install
