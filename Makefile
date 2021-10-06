all: libdragon

V = 1  # force verbose (at least until we have converted all sub-Makefiles)
SOURCE_DIR = $(CURDIR)/src
BUILD_DIR = $(CURDIR)/build
include n64.mk
INSTALLDIR = $(N64_INST)

# Activate N64 toolchain for libdragon build
libdragon: CC=$(N64_CC)
libdragon: AS=$(N64_AS)
libdragon: LD=$(N64_LD)
libdragon: CFLAGS+=$(N64_CFLAGS) -I$(CURDIR)/include
libdragon: ASFLAGS+=$(N64_ASFLAGS) -I$(CURDIR)/include
libdragon: LDFLAGS+=$(N64_LDFLAGS)
libdragon: libdragon.a libdragonsys.a

libdragonsys.a: $(BUILD_DIR)/system.o
	@echo "    [AR] $@"
	$(AR) -rcs -o $@ $^

libdragon.a: $(BUILD_DIR)/n64sys.o $(BUILD_DIR)/interrupt.o \
			 $(BUILD_DIR)/inthandler.o $(BUILD_DIR)/entrypoint.o \
			 $(BUILD_DIR)/debug.o $(BUILD_DIR)/usb.o $(BUILD_DIR)/fatfs/ff.o \
			 $(BUILD_DIR)/fatfs/ffunicode.o $(BUILD_DIR)/dragonfs.o \
			 $(BUILD_DIR)/audio.o $(BUILD_DIR)/display.o \
			 $(BUILD_DIR)/console.o $(BUILD_DIR)/joybus.o \
			 $(BUILD_DIR)/controller.o $(BUILD_DIR)/rtc.o \
			 $(BUILD_DIR)/eepromfs.o $(BUILD_DIR)/mempak.o \
			 $(BUILD_DIR)/tpak.o $(BUILD_DIR)/graphics.o $(BUILD_DIR)/rdp.o \
			 $(BUILD_DIR)/rsp.o $(BUILD_DIR)/dma.o $(BUILD_DIR)/timer.o \
			 $(BUILD_DIR)/exception.o $(BUILD_DIR)/do_ctors.o
	@echo "    [AR] $@"
	$(AR) -rcs -o $@ $^

examples:
	$(MAKE) -C examples
# We are unable to clean examples built with n64.mk unless we
# install it first
examples-clean: install-mk
	$(MAKE) -C examples clean

doxygen: doxygen.conf
	mkdir -p doxygen/
	doxygen doxygen.conf
doxygen-api: doxygen-public.conf
	mkdir -p doxygen/
	doxygen doxygen-public.conf
doxygen-clean:
	rm -rf $(CURDIR)/doxygen

tools:
	$(MAKE) -C tools
tools-install:
	$(MAKE) -C tools install
tools-clean:
	$(MAKE) -C tools clean

install-mk: n64.mk
	install -Cv -m 0644 n64.mk $(INSTALLDIR)/include/n64.mk

install: install-mk libdragon
	install -Cv -m 0644 libdragon.a $(INSTALLDIR)/mips64-elf/lib/libdragon.a
	install -Cv -m 0644 n64.ld $(INSTALLDIR)/mips64-elf/lib/n64.ld
	install -Cv -m 0644 header $(INSTALLDIR)/mips64-elf/lib/header
	install -Cv -m 0644 libdragonsys.a $(INSTALLDIR)/mips64-elf/lib/libdragonsys.a
	install -Cv -m 0644 include/n64sys.h $(INSTALLDIR)/mips64-elf/include/n64sys.h
	install -Cv -m 0644 include/cop0.h $(INSTALLDIR)/mips64-elf/include/cop0.h
	install -Cv -m 0644 include/cop1.h $(INSTALLDIR)/mips64-elf/include/cop1.h
	install -Cv -m 0644 include/interrupt.h $(INSTALLDIR)/mips64-elf/include/interrupt.h
	install -Cv -m 0644 include/dma.h $(INSTALLDIR)/mips64-elf/include/dma.h
	install -Cv -m 0644 include/dragonfs.h $(INSTALLDIR)/mips64-elf/include/dragonfs.h
	install -Cv -m 0644 include/audio.h $(INSTALLDIR)/mips64-elf/include/audio.h
	install -Cv -m 0644 include/display.h $(INSTALLDIR)/mips64-elf/include/display.h
	install -Cv -m 0644 include/debug.h $(INSTALLDIR)/mips64-elf/include/debug.h
	install -Cv -m 0644 include/usb.h $(INSTALLDIR)/mips64-elf/include/usb.h
	install -Cv -m 0644 include/console.h $(INSTALLDIR)/mips64-elf/include/console.h
	install -Cv -m 0644 include/joybus.h $(INSTALLDIR)/mips64-elf/include/joybus.h
	install -Cv -m 0644 include/mempak.h $(INSTALLDIR)/mips64-elf/include/mempak.h
	install -Cv -m 0644 include/controller.h $(INSTALLDIR)/mips64-elf/include/controller.h
	install -Cv -m 0644 include/rtc.h $(INSTALLDIR)/mips64-elf/include/rtc.h
	install -Cv -m 0644 include/eepromfs.h $(INSTALLDIR)/mips64-elf/include/eepromfs.h
	install -Cv -m 0644 include/tpak.h $(INSTALLDIR)/mips64-elf/include/tpak.h
	install -Cv -m 0644 include/graphics.h $(INSTALLDIR)/mips64-elf/include/graphics.h
	install -Cv -m 0644 include/rdp.h $(INSTALLDIR)/mips64-elf/include/rdp.h
	install -Cv -m 0644 include/rsp.h $(INSTALLDIR)/mips64-elf/include/rsp.h
	install -Cv -m 0644 include/timer.h $(INSTALLDIR)/mips64-elf/include/timer.h
	install -Cv -m 0644 include/exception.h $(INSTALLDIR)/mips64-elf/include/exception.h
	install -Cv -m 0644 include/system.h $(INSTALLDIR)/mips64-elf/include/system.h
	install -Cv -m 0644 include/dir.h $(INSTALLDIR)/mips64-elf/include/dir.h
	install -Cv -m 0644 include/libdragon.h $(INSTALLDIR)/mips64-elf/include/libdragon.h
	install -Cv -m 0644 include/ucode.S $(INSTALLDIR)/mips64-elf/include/ucode.S
	install -Cv -m 0644 include/rsp.inc $(INSTALLDIR)/mips64-elf/include/rsp.inc
	install -Cv -m 0644 include/rsp_dma.inc $(INSTALLDIR)/mips64-elf/include/rsp_dma.inc

clean:
	rm -f *.o *.a
	rm -rf $(CURDIR)/build

clobber: clean doxygen-clean examples-clean tools-clean

.PHONY : clobber clean doxygen-clean doxygen doxygen-api examples examples-clean tools tools-clean tools-install

# Automatic dependency tracking
-include $(wildcard $(BUILD_DIR)/*.d)
