all: libdragon

V = 1  # force verbose (at least until we have converted all sub-Makefiles)
SOURCE_DIR = src
BUILD_DIR = build
N64_DSOLDSCRIPT = dso.ld  # Avoid using the (possibly not installed yet) dso.ld from N64_INST
include n64.mk
INSTALLDIR = $(N64_INST)

# N64_INCLUDEDIR is normally (when building roms) a path to the installed include files
# (e.g. /opt/libdragon/$(N64_TARGET)/include), set in n64.mk
# When building libdragon, override it to use the source include files instead (./include)
N64_INCLUDEDIR = $(CURDIR)/include

# N64_BACKTRACE_FILE_PREFIX is exposed from n64.mk, so we can use it to set the
# prefix for libdragon. It is still possible to override this when running make
# for libdragon specifically via a make override.
N64_BACKTRACE_FILE_PREFIX=libdragon

LIBDRAGON_CFLAGS = -I$(CURDIR)/src -D__LIBDRAGON_INTERNAL_BUILD

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
libdragon: libdragon.a libdragonsys.a gen-version

libdragonsys.a: $(BUILD_DIR)/system.o

LIBDRAGON_OBJS += \
	$(BUILD_DIR)/accounting.o \
	$(BUILD_DIR)/profile.o \
	$(BUILD_DIR)/n64sys.o \
	$(BUILD_DIR)/scratch.o \
	$(BUILD_DIR)/vaddr64.o \
	$(BUILD_DIR)/mi_memset.o \
	$(BUILD_DIR)/interrupt.o \
	$(BUILD_DIR)/backtrace.o \
	$(BUILD_DIR)/symtable.o \
	$(BUILD_DIR)/dir.o \
	$(BUILD_DIR)/inthandler.o \
	$(BUILD_DIR)/entrypoint.o \
	$(BUILD_DIR)/entropy.o \
	$(BUILD_DIR)/rand.o \
	$(BUILD_DIR)/utils.o \
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
	$(BUILD_DIR)/lspr3.o \
	$(BUILD_DIR)/lspr1.o \
	$(BUILD_DIR)/rsp_lspr1.o \
	$(BUILD_DIR)/dma.o \
	$(BUILD_DIR)/timer.o \
	$(BUILD_DIR)/exception.o \
	$(BUILD_DIR)/do_ctors.o \
	$(BUILD_DIR)/dlfcn.o \
	$(BUILD_DIR)/hashtable.o \
	$(BUILD_DIR)/string_hash.o \
	$(BUILD_DIR)/model64.o \
	$(BUILD_DIR)/a3d.o \
	$(BUILD_DIR)/sram.o \
	$(BUILD_DIR)/flashram.o \
	$(BUILD_DIR)/ucontext.o \
	$(BUILD_DIR)/ucontext_asm.o \
	$(BUILD_DIR)/coroutine.o

include $(SOURCE_DIR)/kernel/libdragon.mk
include $(SOURCE_DIR)/audio/libdragon.mk
include $(SOURCE_DIR)/bb/libdragon.mk
include $(SOURCE_DIR)/dd/libdragon.mk
include $(SOURCE_DIR)/joybus/libdragon.mk
include $(SOURCE_DIR)/GL/libdragon.mk
include $(SOURCE_DIR)/video/libdragon.mk
include $(SOURCE_DIR)/rspq/libdragon.mk
include $(SOURCE_DIR)/rdpq/libdragon.mk
include $(SOURCE_DIR)/magma/libdragon.mk
include $(SOURCE_DIR)/math/libdragon.mk
include $(SOURCE_DIR)/compress/libdragon.mk

# TODO: Make this generically available in n64.mk somehow
$(SOURCE_DIR)/magma/rsp_magma.h: $(BUILD_DIR)/magma/rsp_magma.o
	$(N64_OBJDUMP) -t $(BUILD_DIR)/magma/rsp_magma.elf \
		| awk 'BEGIN {print("#ifndef __RSP_MAGMA_SYMBOLS\n#define __RSP_MAGMA_SYMBOLS") } $$3 ~ /\.data|\.text/ {printf("#define RSP_MAGMA_%s 0x%s\n", $$5, substr($$1,5,4))} END {print("#endif")}' \
		> $@

$(BUILD_DIR)/magma/magma.o: $(SOURCE_DIR)/magma/rsp_magma.h

libdragon: $(LIBDRAGON_DSOS)
$(BUILD_DIR)/dso/%.o: $(SOURCE_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "    [CC] $<"
	$(CC) -c $(CFLAGS) -G 0 -fvisibility=hidden -o $@ $<

libdragon.a: $(LIBDRAGON_OBJS)

%.a:
	@echo "    [AR] $@"
	rm -f $@
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

# Install n64.mk to the install directory, always. This phony target does not
# do any check and will always copy the file. This is what we need because the
# installed file can be newer (timestamp wise) but different (eg: coming from
# another branch).
install-mk:
	@echo "    [INSTALL] n64.mk"
	mkdir -p $(INSTALLDIR)/include
	install -cv -m 0644 n64.mk $(INSTALLDIR)/include/n64.mk

# This target is just a convenience target to install n64.mk in case it doesn't
# exist yet, and it's only used by the clean targets that would otherwise fail.
# This also allows to try running those targets without sudo, as a copy isn't
# always required.
$(INSTALLDIR)/include/n64.mk: n64.mk
	mkdir -p $(INSTALLDIR)/include
	install -cv -m 0644 n64.mk $(INSTALLDIR)/include/n64.mk

gen-version:
# Generate a version file for libdragon. We go through git archive so that
# the export-subst is applied to the template file.
# If .git doesn't exist, assume export-subst ran and just copy the file.
# Otherwise, use git-archive to generate the subst'd version file.
# NOTE: git can fail to access the repository via sudo for security/permissions
# reasons (for instance, it happens on Mac when using sudo on an external volume).
# We check for this via git rev-parse. In these cases, we hope the file was
# generated without sudo as part of the normal build process.
	@mkdir -p $(BUILD_DIR)
	if [ -e .git ]; then \
		if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then \
			git archive --format=tar HEAD libdragon.version \
				| tar -xOf - libdragon.version > "$(BUILD_DIR)/libdragon.version"; \
			if ! git diff-index --quiet HEAD -- 2>/dev/null; then \
				sed 's/"dirty":[[:space:]]*false/"dirty": true/' "$(BUILD_DIR)/libdragon.version" > "$(BUILD_DIR)/version.tmp"; \
				mv -f "$(BUILD_DIR)/version.tmp" "$(BUILD_DIR)/libdragon.version"; \
			fi; \
		else \
			if [ ! -e "$(BUILD_DIR)/libdragon.version" ]; then \
				echo "WARNING: .git exists but git refuses to access it (permission problems?)" >&2; \
				echo "WARNING: libdragon.version will not be generated." >&2; \
			fi; \
		fi; \
	else \
		cp libdragon.version "$(BUILD_DIR)/libdragon.version"; \
	fi;

install: install-mk libdragon
	@echo "    [INSTALL] libdragon"
	mkdir -p $(INSTALLDIR)/$(N64_TARGET)/lib
	install -Cv -m 0644 libdragon.a $(INSTALLDIR)/$(N64_TARGET)/lib/libdragon.a
	install -Cv -m 0644 n64.ld $(INSTALLDIR)/$(N64_TARGET)/lib/n64.ld
	install -Cv -m 0644 dso.ld $(INSTALLDIR)/$(N64_TARGET)/lib/dso.ld
	install -Cv -m 0644 rsp.ld $(INSTALLDIR)/$(N64_TARGET)/lib/rsp.ld
	install -Cv -m 0644 libdragonsys.a $(INSTALLDIR)/$(N64_TARGET)/lib/libdragonsys.a
	mkdir -p $(INSTALLDIR)/$(N64_TARGET)/lib/dso
	install -Cv -m 0644 $(LIBDRAGON_DSOS) $(LIBDRAGON_DSOS:.dso=.dso.sym) $(INSTALLDIR)/$(N64_TARGET)/lib/dso/
	@echo "    [INSTALL] libdragon.version"
	mkdir -p $(INSTALLDIR)/$(N64_TARGET)/include
	if [ -f "$(BUILD_DIR)/libdragon.version" ]; then \
		install -Cv -m 0644 $(BUILD_DIR)/libdragon.version $(INSTALLDIR)/$(N64_TARGET)/include/; \
	else \
		rm -f $(INSTALLDIR)/$(N64_TARGET)/include/libdragon.version; \
	fi;
	@echo "    [INSTALL] include/*.h"
	install -Cv -m 0644 include/*.h $(INSTALLDIR)/$(N64_TARGET)/include/
	install -Cv -m 0644 include/*.inc $(INSTALLDIR)/$(N64_TARGET)/include/
	install -Cv -m 0644 include/ucode.S $(INSTALLDIR)/$(N64_TARGET)/include/
	mkdir -p $(INSTALLDIR)/$(N64_TARGET)/include/GL
	install -Cv -m 0644 include/GL/*.h $(INSTALLDIR)/$(N64_TARGET)/include/GL/
	mkdir -p $(INSTALLDIR)/$(N64_TARGET)/include/newlib_overrides
	install -Cv -m 0644 include/newlib_overrides/*.h $(INSTALLDIR)/$(N64_TARGET)/include/newlib_overrides/
	mkdir -p $(INSTALLDIR)/$(N64_TARGET)/include/newlib_overrides/sys
	install -Cv -m 0644 include/newlib_overrides/sys/*.h $(INSTALLDIR)/$(N64_TARGET)/include/newlib_overrides/sys/
	mkdir -p $(INSTALLDIR)/$(N64_TARGET)/include/libcart
	install -Cv -m 0644 src/libcart/cart.h $(INSTALLDIR)/$(N64_TARGET)/include/libcart/cart.h
	mkdir -p $(INSTALLDIR)/$(N64_TARGET)/include/fatfs
	install -Cv -m 0644 src/fatfs/diskio.h $(INSTALLDIR)/$(N64_TARGET)/include/fatfs/diskio.h
	install -Cv -m 0644 src/fatfs/ff.h $(INSTALLDIR)/$(N64_TARGET)/include/fatfs/ff.h
	install -Cv -m 0644 src/fatfs/ffconf.h $(INSTALLDIR)/$(N64_TARGET)/include/fatfs/ffconf.h

clean:
	rm -f *.o *.a
	rm -rf $(CURDIR)/build

regen:
# Regenerate generated files that are committed. If they are changed, they will
# be marked as modified in git.
	cd $(SOURCE_DIR)/rdpq && ./mkfontbuiltin.sh

test:
	$(MAKE) -C tests
	$(MAKE) -C tests/cpakfs test
	$(MAKE) -C tests/hashtable test
	$(MAKE) -C tests/preview test
	python3 -m unittest discover tools/cpaktool/tests

test-clean: $(INSTALLDIR)/include/n64.mk
	$(MAKE) -C tests clean

clobber: clean examples-clean tools-clean test-clean

.PHONY : clobber clean doxygen-api examples examples-clean tools tools-clean tools-install test test-clean install-mk libdragon gen-version

# Automatic dependency tracking
-include $(wildcard $(BUILD_DIR)/*.d) $(wildcard $(BUILD_DIR)/*/*.d)
