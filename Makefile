ROOTDIR = $(N64_INST)
CFLAGS = -std=gnu99 -O2 -G0 -Wall -Werror -mtune=vr4300 -march=vr4300 -I$(CURDIR)/include -I$(ROOTDIR)/include -I$(ROOTDIR)/mips64-elf/include
ASFLAGS = -mtune=vr4300 -march=vr4300
N64PREFIX = $(N64_INST)/bin/mips64-elf-
INSTALLDIR = $(N64_INST)
CC = $(N64PREFIX)gcc
AS = $(N64PREFIX)as
LD = $(N64PREFIX)ld
AR = $(N64PREFIX)ar

all: tools libdragon

tools: 
	make -C tools/dumpdfs
	make -C tools/mkdfs
	make -C tools/mksprite

libdragon: libdragon.a

include files.in

libdragon.a: $(OFILES)
	$(AR) -rcs -o libdragon.a $(OFILES)

(CURDIR)/build/:
	mkdir $(CURDIR)/build

install: libdragon.a
	install -D --mode=644 libdragon.a $(INSTALLDIR)/lib/libdragon.a
	install -D --mode=644 include/dragonfs.h $(INSTALLDIR)/include/dragonfs.h
	install -D --mode=644 include/audio.h $(INSTALLDIR)/include/audio.h
	install -D --mode=644 include/display.h $(INSTALLDIR)/include/display.h
	install -D --mode=644 include/console.h $(INSTALLDIR)/include/console.h
	install -D --mode=644 include/controller.h $(INSTALLDIR)/include/controller.h
	install -D --mode=644 include/graphics.h $(INSTALLDIR)/include/graphics.h

install-tools: tools
	install -D --mode=755 tools/dumpdfs/dumpdfs $(INSTALLDIR)/bin/dumpdfs
	install -D --mode=755 tools/mksprite/mksprite $(INSTALLDIR)/bin/mksprite
	install -D --mode=755 tools/mkdfs/mkdfs $(INSTALLDIR)/bin/mkdfs

clean:
	rm -f *.o *.a
	rm -f $(CURDIR)/build/*

rclean: clean
	rm -f $(CURDIR)/tools/dumpdfs/dumpdfs
	rm -f $(CURDIR)/tools/dumpdfs/dumpdfs.exe
	rm -f $(CURDIR)/tools/mkdfs/mkdfs
	rm -f $(CURDIR)/tools/mkdfs/mkdfs.exe
	rm -f $(CURDIR)/tools/mksprite/mksprite
	rm -f $(CURDIR)/tools/mksprite/mksprite.exe

.PHONY : clean rclean tools
