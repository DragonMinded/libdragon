ROOTDIR = $(N64_INST)
CFLAGS = -std=gnu99 -O1 -G0 -Wall -Werror -mtune=vr4300 -march=vr4300 -I$(CURDIR)/include -I$(ROOTDIR)/include -I$(ROOTDIR)/mips64-elf/include
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

libdragon.a: $(OFILES_LD)
	$(AR) -rcs -o libdragon.a $(OFILES_LD)

libdragonsys.a: $(OFILES_LDS)
	$(AR) -rcs -o libdragonsys.a $(OFILES_LDS)

libdragonpp.a: $(OFILES_LDP)
	$(AR) -rcs -o libdragonpp.a $(OFILES_LDP)

(CURDIR)/build/:
	mkdir $(CURDIR)/build

install: libdragon.a libdragonsys.a libdragonpp.a
	install -D --mode=644 libdragon.a $(INSTALLDIR)/lib/libdragon.a
	install -D --mode=644 n64ld.x $(INSTALLDIR)/lib/n64ld.x
	install -D --mode=644 n64ld_cpp.x $(INSTALLDIR)/lib/n64ld_cpp.x
	install -D --mode=644 n64ld_exp_cpp.x $(INSTALLDIR)/lib/n64ld_exp_cpp.x
	install -D --mode=644 header $(INSTALLDIR)/lib/header
	install -D --mode=644 libdragonsys.a $(INSTALLDIR)/lib/libdragonsys.a
	install -D --mode=644 libdragonpp.a $(INSTALLDIR)/lib/libdragonpp.a
	install -D --mode=644 include/n64sys.h $(INSTALLDIR)/include/n64sys.h
	install -D --mode=644 include/interrupt.h $(INSTALLDIR)/include/interrupt.h
	install -D --mode=644 include/dma.h $(INSTALLDIR)/include/dma.h
	install -D --mode=644 include/dragonfs.h $(INSTALLDIR)/include/dragonfs.h
	install -D --mode=644 include/audio.h $(INSTALLDIR)/include/audio.h
	install -D --mode=644 include/display.h $(INSTALLDIR)/include/display.h
	install -D --mode=644 include/console.h $(INSTALLDIR)/include/console.h
	install -D --mode=644 include/controller.h $(INSTALLDIR)/include/controller.h
	install -D --mode=644 include/graphics.h $(INSTALLDIR)/include/graphics.h
	install -D --mode=644 include/rdp.h $(INSTALLDIR)/include/rdp.h
	install -D --mode=644 include/timer.h $(INSTALLDIR)/include/timer.h
	install -D --mode=644 include/exception.h $(INSTALLDIR)/include/exception.h
	install -D --mode=644 include/system.h $(INSTALLDIR)/include/system.h
	install -D --mode=644 include/dir.h $(INSTALLDIR)/include/dir.h
	install -D --mode=644 include/libdragon.h $(INSTALLDIR)/include/libdragon.h

clean:
	rm -f *.o *.a
	rm -f $(CURDIR)/build/*

.PHONY : clean
