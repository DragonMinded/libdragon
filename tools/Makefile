N64_GCCPREFIX ?= $(N64_INST)
N64_TARGET ?= mips64-elf
INSTALLDIR ?= $(N64_INST)
N64_GCCPREFIX_TRIPLET = $(N64_GCCPREFIX)/bin/$(addsuffix -,$(N64_TARGET))
TARGET := $(shell $(CC) -dumpmachine)

C_CXX_FLAGS += -O2 -pthread -Wall -Werror
C_CXX_FLAGS += -I../include -MMD
C_CXX_FLAGS += -fdiagnostics-color=always
C_CXX_FLAGS += -Wno-unused-result -Wno-error=unknown-pragmas -Wno-sign-compare

LDFLAGS += -pthread

CFLAGS   += -std=gnu11 $(C_CXX_FLAGS) 
CXXFLAGS += -std=gnu++17 $(C_CXX_FLAGS) 
CXXFLAGS += -Wno-c++11-narrowing -Wno-narrowing -Wno-error=conversion-null

ifeq ($(OS),Windows_NT)
	CFLAGS += -static
	CXXFLAGS += -static
  	LDFLAGS += -static

# Check that $CC and $CXX are set to a mingw compiler. Many users are not really
# familiar with msys2 and they tend to have weirdly broken environments. Let's try
# to help them.
	CHECK_CC := $(shell $(CC) -dumpmachine 2>/dev/null | grep -i "mingw" >/dev/null && echo ok || echo fail)
	CHECK_CXX := $(shell $(CXX) -dumpmachine 2>/dev/null | grep -i "mingw" >/dev/null && echo ok || echo fail)
	ifeq ($(CHECK_CC),fail)
		$(error "CC is not set to a mingw compiler: ${CC}")
	endif
	ifeq ($(CHECK_CXX),fail)
		$(error "CXX is not set to a mingw compiler: ${CXX}")
	endif
endif

all:

%.o: %.c
	@echo "    [CC] $@"
	$(CCACHE) $(CC) $(CFLAGS) -c -o $@ $<
%.o: %.cpp
	@echo "    [CXX] $@"
	$(CCACHE) $(CXX) $(CXXFLAGS) -c -o $@ $<
%.a:
	@echo "    [AR] $@"
	rm -f $@
	$(AR) rcs $@ $^

# Avoid many warnings for vendored code that we don't intend to ever modify
common/shrinkler_compress.o: 	   CFLAGS += -Wno-all -Wno-error
mkfont/freetype/FreeTypeAmalgam.o: CFLAGS += -Wno-all -Wno-error
mkfont/plutosvg/plutosvg-amalgam.o:CFLAGS += -Imkfont/plutosvg -Wno-unused-function -Wno-error
mkfont/mkfont.o:              	   CXXFLAGS += -Imkfont/plutosvg -Imkfont/freetype


common-clean:
	rm -f common/*.o common/*.a common/*.d
common/assetcomp.a: common/assetcomp.o common/lz4_compress.o \
				    common/aplib_compress.o common/shrinkler_compress.o

DECOMP_STUBS=common/mips_decomp_l1.bin common/mips_decomp_l2.bin common/mips_decomp_l3.bin

common/mips_decomp_l1.bin: ../src/compress/lz4_dec_fast.S
common/mips_decomp_l2.bin: ../src/compress/aplib_dec_fast.S
common/mips_decomp_l3.bin: ../src/compress/shrinkler_dec_fast.S

$(DECOMP_STUBS):
	@echo "    [MIPS] $@"
	$(N64_GCCPREFIX_TRIPLET)gcc -c -o $@.o $<
	$(N64_GCCPREFIX_TRIPLET)objcopy -O binary $@.o $@
	rm $@.o

-include $(wildcard common/*.d)

mkasset_OBJS = mkasset/mkasset.o common/assetcomp.a
mksprite_OBJS = mksprite/mksprite.o common/assetcomp.a
audioconv64_OBJS = audioconv64/audioconv64.o
mkdfs_OBJS = mkdfs/mkdfs.o
mkfont_OBJS = mkfont/mkfont.o mkfont/freetype/FreeTypeAmalgam.o mkfont/plutosvg/plutosvg-amalgam.o common/assetcomp.a
n64dso_OBJS = n64dso/n64dso.o common/assetcomp.a
n64dso-extern_OBJS = n64dso/n64dso-extern.o
n64dso-msym_OBJS = n64dso/n64dso-msym.o
dumpdfs_OBJS = dumpdfs/dumpdfs.o
n64tool_OBJS = n64tool.o
n64sym_OBJS = n64sym.o
ed64romconfig_OBJS = ed64romconfig.o
n64elfcompress_OBJS = n64elfcompress/n64elfcompress.o common/assetcomp.a
n64elfcompress/n64elfcompress.o: n64elfcompress/n64elfcompress.c $(DECOMP_STUBS)

TOOLS = n64tool n64sym n64elfcompress ed64romconfig audioconv64 mkdfs dumpdfs mkasset mksprite mkfont n64dso n64dso-msym n64dso-extern

# Define a variable that has value ".exe" on Windows and "" on other platforms
EXE = $(if $(findstring Windows,$(OS)),.exe,)

define TOOL_template
.PHONY: $(1)-install $(1)-clean
$(1)_DIR ?= $$(dir $$(firstword $$($(1)_OBJS)))
$(1)_BIN ?= $$($(1)_DIR)$(1)$(EXE)
ifneq ($$($(1)_DIR),./)
.PHONY: $(1)
$(1): $$($(1)_BIN)
endif
$$($(1)_BIN): $$($(1)_OBJS)
	@echo "    [TOOL] $(1)"
	$(CXX) $(LDFLAGS) -o $$@ $$^
$(1)-install: $(1)
	mkdir -p $(INSTALLDIR)/bin
	install -m 0755 $$($(1)_BIN) $(INSTALLDIR)/bin
$(1)-clean:
	rm -f $$($(1)_BIN) $$($(1)_DIR)/**/*.o $$($(1)_DIR)/**/*.d
-include $$(wildcard $$($(1)_DIR)/*.d)
endef

$(foreach tool,$(TOOLS),$(eval $(call TOOL_template,$(tool))))
all: $(TOOLS)
install: $(foreach tool,$(TOOLS),$(tool)-install)
clean: $(foreach tool,$(TOOLS),$(tool)-clean) common-clean
	rm -f ${n64tool_OBJS} ${n64sym_OBJS} ${ed64romconfig_OBJS} 
.PHONY: all install clean

ifneq ($(V),1)
.SILENT:
endif
