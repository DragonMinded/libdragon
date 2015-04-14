ROOTDIR = $(N64_INST)
CFLAGS = -std=gnu99 -O2 -G0 -Wall -Werror -mtune=vr4300 -march=vr4300 -I$(CURDIR)/include -I$(ROOTDIR)/include -I$(ROOTDIR)/mips64-elf/include
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
	make -C tools
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
	install -m 0644 libdragon.a $(INSTALLDIR)/lib/libdragon.a
	install -m 0644 n64ld.x $(INSTALLDIR)/lib/n64ld.x
	install -m 0644 n64ld_cpp.x $(INSTALLDIR)/lib/n64ld_cpp.x
	install -m 0644 n64ld_exp_cpp.x $(INSTALLDIR)/lib/n64ld_exp_cpp.x
	install -m 0644 header $(INSTALLDIR)/lib/header
	install -m 0644 libdragonsys.a $(INSTALLDIR)/lib/libdragonsys.a
	install -m 0644 libdragonpp.a $(INSTALLDIR)/lib/libdragonpp.a
	install -m 0644 include/n64sys.h $(INSTALLDIR)/include/n64sys.h
	install -m 0644 include/interrupt.h $(INSTALLDIR)/include/interrupt.h
	install -m 0644 include/dma.h $(INSTALLDIR)/include/dma.h
	install -m 0644 include/dragonfs.h $(INSTALLDIR)/include/dragonfs.h
	install -m 0644 include/audio.h $(INSTALLDIR)/include/audio.h
	install -m 0644 include/display.h $(INSTALLDIR)/include/display.h
	install -m 0644 include/console.h $(INSTALLDIR)/include/console.h
	install -m 0644 include/mempak.h $(INSTALLDIR)/include/mempak.h
	install -m 0644 include/controller.h $(INSTALLDIR)/include/controller.h
	install -m 0644 include/graphics.h $(INSTALLDIR)/include/graphics.h
	install -m 0644 include/rdp.h $(INSTALLDIR)/include/rdp.h
	install -m 0644 include/timer.h $(INSTALLDIR)/include/timer.h
	install -m 0644 include/exception.h $(INSTALLDIR)/include/exception.h
	install -m 0644 include/system.h $(INSTALLDIR)/include/system.h
	install -m 0644 include/dir.h $(INSTALLDIR)/include/dir.h
	install -m 0644 include/libdragon.h $(INSTALLDIR)/include/libdragon.h

clean:
	rm -f *.o *.a
	rm -rf $(CURDIR)/build

clobber: clean doxygen-clean examples-clean tools-clean

.PHONY : clobber clean doxygen-clean doxygen doxygen-api examples examples-clean tools tools-clean tools-install
