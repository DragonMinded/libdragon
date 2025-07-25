all: libdragon

V = 1  # force verbose (at least until we have converted all sub-Makefiles)
SOURCE_DIR = src
BUILD_DIR = build
include n64.mk
INSTALLDIR = $(N64_INST)

# N64_INCLUDEDIR is normally (when building roms) a path to the installed include files
# (e.g. /opt/libdragon/mips64-elf/include), set in n64.mk
# When building libdragon, override it to use the source include files instead (./include)
N64_INCLUDEDIR = $(CURDIR)/include

# N64_BACKTRACE_FILE_PREFIX is exposed from n64.mk, so we can use it to set the
# prefix for libdragon. It is still possible to override this when running make
# for libdragon specifically via a make override.
N64_BACKTRACE_FILE_PREFIX=libdragon

LIBDRAGON_CFLAGS = -I$(CURDIR)/src

# Activate N64 toolchain for libdragon build
libdragon: CC=$(N64_CC)
libdragon: CXX=$(N64_CXX)
libdragon: AS=$(N64_AS)
libdragon: LD=$(N64_LD)
libdragon: CFLAGS+=$(N64_CFLAGS) $(LIBDRAGON_CFLAGS)
libdragon: CXXFLAGS+=$(N64_CXXFLAGS) $(LIBDRAGON_CFLAGS)
libdragon: ASFLAGS+=$(N64_ASFLAGS) $(LIBDRAGON_CFLAGS)
libdragon: RSPASFLAGS+=$(N64_RSPASFLAGS) $(LIBDRAGON_CFLAGS)
libdragon: LDFLAGS+=$(N64_LDFLAGS)
libdragon: libdragon.a libdragonsys.a

libdragonsys.a: $(BUILD_DIR)/system.o

LIBDRAGON_OBJS += \
	$(BUILD_DIR)/n64sys.o \
	$(BUILD_DIR)/interrupt.o \
	$(BUILD_DIR)/backtrace.o \
	$(BUILD_DIR)/dir.o \
	$(BUILD_DIR)/inthandler.o \
	$(BUILD_DIR)/entrypoint.o \
	$(BUILD_DIR)/debug.o \
	$(BUILD_DIR)/debugcpp.o \
	$(BUILD_DIR)/usb.o \
	$(BUILD_DIR)/libcart/cart.o \
	$(BUILD_DIR)/fatfs/ff.o \
	$(BUILD_DIR)/fatfs/ffunicode.o \
	$(BUILD_DIR)/fat.o \
	$(BUILD_DIR)/rompak.o \
	$(BUILD_DIR)/dragonfs.o \
	$(BUILD_DIR)/audio.o \
	$(BUILD_DIR)/vi.o \
	$(BUILD_DIR)/eia608.o \
	$(BUILD_DIR)/display.o \
	$(BUILD_DIR)/surface.o \
	$(BUILD_DIR)/console.o \
	$(BUILD_DIR)/asset.o \
	$(BUILD_DIR)/pifile.o \
	$(BUILD_DIR)/ed64x.o \
	$(BUILD_DIR)/ed64.o \
	$(BUILD_DIR)/rtc.o \
	$(BUILD_DIR)/rtc_internal.o \
	$(BUILD_DIR)/graphics.o \
	$(BUILD_DIR)/rdp.o \
	$(BUILD_DIR)/rsp.o \
	$(BUILD_DIR)/rsp_crash.o \
	$(BUILD_DIR)/inspector.o \
	$(BUILD_DIR)/sprite.o \
	$(BUILD_DIR)/dma.o \
	$(BUILD_DIR)/timer.o \
	$(BUILD_DIR)/exception.o \
	$(BUILD_DIR)/do_ctors.o \
	$(BUILD_DIR)/dlfcn.o \
	$(BUILD_DIR)/hashtable.o \
	$(BUILD_DIR)/model64.o

include $(SOURCE_DIR)/kernel/libdragon.mk
include $(SOURCE_DIR)/audio/libdragon.mk
include $(SOURCE_DIR)/bb/libdragon.mk
include $(SOURCE_DIR)/dd/libdragon.mk
include $(SOURCE_DIR)/joybus/libdragon.mk
include $(SOURCE_DIR)/GL/libdragon.mk
include $(SOURCE_DIR)/video/libdragon.mk
include $(SOURCE_DIR)/rspq/libdragon.mk
include $(SOURCE_DIR)/rdpq/libdragon.mk
include $(SOURCE_DIR)/math/libdragon.mk
include $(SOURCE_DIR)/compress/libdragon.mk

libdragon.a: $(LIBDRAGON_OBJS)

%.a:
	@echo "    [AR] $@"
	$(N64_AR) -rcs -o $@ $^

examples:
	$(MAKE) -C examples
# We are unable to clean examples built with n64.mk unless we
# install it first
examples-clean: $(INSTALLDIR)/include/n64.mk
	$(MAKE) -C examples clean

doxygen-api: doxygen-public.conf
	doxygen doxygen-public.conf

tools:
	$(MAKE) -C tools
tools-install:
	$(MAKE) -C tools install
tools-clean:
	$(MAKE) -C tools clean

install-mk: $(INSTALLDIR)/include/n64.mk

$(INSTALLDIR)/include/n64.mk: n64.mk
# Always update timestamp of n64.mk. This make sure that further targets
# depending on install-mk won't always try to re-install it.
	mkdir -p $(INSTALLDIR)/include
	install -cv -m 0644 n64.mk $(INSTALLDIR)/include/n64.mk

install: install-mk libdragon
	mkdir -p $(INSTALLDIR)/mips64-elf/lib
	install -Cv -m 0644 libdragon.a $(INSTALLDIR)/mips64-elf/lib/libdragon.a
	install -Cv -m 0644 n64.ld $(INSTALLDIR)/mips64-elf/lib/n64.ld
	install -Cv -m 0644 dso.ld $(INSTALLDIR)/mips64-elf/lib/dso.ld
	install -Cv -m 0644 rsp.ld $(INSTALLDIR)/mips64-elf/lib/rsp.ld
	install -Cv -m 0644 libdragonsys.a $(INSTALLDIR)/mips64-elf/lib/libdragonsys.a
	mkdir -p $(INSTALLDIR)/mips64-elf/include
	install -Cv -m 0644 include/*.h $(INSTALLDIR)/mips64-elf/include/
	install -Cv -m 0644 include/*.inc $(INSTALLDIR)/mips64-elf/include/
	install -Cv -m 0644 include/ucode.S $(INSTALLDIR)/mips64-elf/include/
	mkdir -p $(INSTALLDIR)/mips64-elf/include/GL
	install -Cv -m 0644 include/GL/*.h $(INSTALLDIR)/mips64-elf/include/GL/
	mkdir -p $(INSTALLDIR)/mips64-elf/include/newlib_overrides
	install -Cv -m 0644 include/newlib_overrides/*.h $(INSTALLDIR)/mips64-elf/include/newlib_overrides/
	mkdir -p $(INSTALLDIR)/mips64-elf/include/libcart
	install -Cv -m 0644 src/libcart/cart.h $(INSTALLDIR)/mips64-elf/include/libcart/cart.h
	mkdir -p $(INSTALLDIR)/mips64-elf/include/fatfs
	install -Cv -m 0644 src/fatfs/diskio.h $(INSTALLDIR)/mips64-elf/include/fatfs/diskio.h
	install -Cv -m 0644 src/fatfs/ff.h $(INSTALLDIR)/mips64-elf/include/fatfs/ff.h
	install -Cv -m 0644 src/fatfs/ffconf.h $(INSTALLDIR)/mips64-elf/include/fatfs/ffconf.h

clean:
	rm -f *.o *.a
	rm -rf $(CURDIR)/build

regen:
# Regenerate generated files that are committed. If they are changed, they will
# be marked as modified in git.
	cd $(SOURCE_DIR)/rdpq && ./mkfontbuiltin.sh

test:
	$(MAKE) -C tests

test-clean: install-mk
	$(MAKE) -C tests clean

clobber: clean examples-clean tools-clean test-clean

.PHONY : clobber clean doxygen-api examples examples-clean tools tools-clean tools-install test test-clean install-mk

# Automatic dependency tracking
-include $(wildcard $(BUILD_DIR)/*.d) $(wildcard $(BUILD_DIR)/*/*.d)
