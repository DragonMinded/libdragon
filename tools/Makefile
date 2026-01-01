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

# With mingw (even if crosscompiling), we need to link with ntdll
# and produce .exe files
ifeq ($(findstring mingw,$(TARGET)),mingw)
LDLIBS += -lntdll
EXE = .exe
else
LDLIBS = 
EXE = 
endif

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

# Decomment to rebuild tools with ASAN
#C_CXX_FLAGS += -O0 -g -fsanitize=address
#LDFLAGS += -g -fsanitize=address

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
mkfont_OBJS = mkfont/mkfont.o mkfont/freetype/FreeTypeAmalgam.o mkfont/plutosvg/plutosvg-amalgam.o common/assetcomp.a
mkmodel_OBJS = mkmodel/mkmodel.o common/assetcomp.a
mkmaterial_OBJS = mkmaterial/mkmaterial.o mkmaterial/mkmaterial_parse.o mkmaterial/mkmaterial_export.o common/assetcomp.a 
combexpr_OBJS = mkmaterial/combexpr_cli.o mkmaterial/combexpr_disasm.o
n64dso_OBJS = n64dso/n64dso.o common/assetcomp.a
n64dso-extern_OBJS = n64dso/n64dso-extern.o
n64dso-msym_OBJS = n64dso/n64dso-msym.o
audioconv64_OBJS = audioconv64/audioconv64.o audioconv64/libvadpcm.o audioconv64/libopus.o audioconv64/libsamplerate.o audioconv64/liblzh5.o audioconv64/libxm.o common/assetcomp.a
videoconv64_OBJS = videoconv64/videoconv64.o videoconv64/vconv_analyze.o videoconv64/vconv_encode.o videoconv64/vconv_encode_mpeg1.o videoconv64/vconv_encode_h264.o videoconv64/vconv_audio.o videoconv64/vconv_subtitles.o videoconv64/vconv_utils.o videoconv64/vconv_seek.o common/binout.o common/assetcomp.a
rdpvalidate_OBJS = rdpvalidate/rdpvalidate.o
mkdfs_OBJS = mkdfs/mkdfs.o
dumpdfs_OBJS = dumpdfs/dumpdfs.o
cpaktool_OBJS = cpaktool/cpaktool.o cpaktool/cpakcmds.o cpaktool/cpaklib.o cpaktool/cpakwrapper.o
n64tool_OBJS = n64tool.o
n64sym_OBJS = n64sym/n64sym.o n64sym/n64sym_huffman.o
n64symdump_OBJS = n64sym/n64symdump.o n64sym/n64sym_huffman.o
n64metadata_OBJS = n64metadata.o
ed64romconfig_OBJS = ed64romconfig.o
n64elfcompress_OBJS = n64elfcompress/n64elfcompress.o common/assetcomp.a
n64elfcompress/n64elfcompress.o: n64elfcompress/n64elfcompress.c $(DECOMP_STUBS)
TOOLS = n64tool n64metadata n64sym n64symdump n64elfcompress ed64romconfig audioconv64 videoconv64 mkdfs dumpdfs mkasset mksprite mkfont mkmodel mkmaterial n64dso n64dso-msym n64dso-extern rdpvalidate combexpr cpaktool

# Define a variable that has value ".exe" when building on Windows
# Alo
ifeq ($(OS),Windows_NT)
MANIFEST = $(if $(findstring Windows,$(OS)),manifest.o,)
manifest.o: manifest.rc tool.manifest
	@echo "    [RC] $@"
	windres manifest.rc -O coff -o $@
endif

define TOOL_template
.PHONY: $(1)-install $(1)-clean
$(1)_DIR ?= $$(dir $$(firstword $$($(1)_OBJS)))
$(1)_BIN ?= $$($(1)_DIR)$(1)$(EXE)
ifneq ($$($(1)_DIR),./)
.PHONY: $(1)
$(1): $$($(1)_BIN)
endif
ifneq ($(EXE),)
.PHONY: $(1)
$(1): $$($(1)_BIN)
endif
$$($(1)_BIN): $$($(1)_OBJS) ${MANIFEST}
	@echo "    [TOOL] $(1)"
	$(CXX) $(LDFLAGS) -o $$@ $$^ $(LDLIBS)
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
	rm -f ${n64tool_OBJS} ${n64metadata_OBJS} ${n64sym_OBJS} ${n64symdump_OBJS} ${ed64romconfig_OBJS} 
.PHONY: all install clean

ifneq ($(V),1)
.SILENT:
endif
