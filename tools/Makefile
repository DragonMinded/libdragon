N64_GCCPREFIX ?= $(N64_INST)
INSTALLDIR ?= $(N64_INST)
CFLAGS   += -std=gnu11 -O2 -Wall -Werror -Wno-unused-result -Wno-error=unknown-pragmas -Wno-error=sign-compare -I../include -MMD
CXXFLAGS += -std=c++11 -O2 -Wall -Werror -Wno-unused-result -Wno-error=unknown-pragmas -Wno-error=sign-compare -MMD
LDFLAGS   = 

all:

%.o: %.c
	@echo "    [CC] $@"
	$(CC) $(CFLAGS) -c -o $@ $<
%.o: %.cpp
	@echo "    [CXX] $@"
	$(CXX) $(CXXFLAGS) -c -o $@ $<
%.a:
	@echo "    [AR] $@"
	rm -f $@
	$(AR) rcs $@ $^

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
	$(N64_GCCPREFIX)/bin/mips64-elf-gcc -c -o $@.o $<
	$(N64_GCCPREFIX)/bin/mips64-elf-objcopy -O binary $@.o $@
	rm $@.o

-include $(wildcard common/*.d)

mkasset_OBJS = mkasset/mkasset.o common/assetcomp.a
mksprite_OBJS = mksprite/mksprite.o common/assetcomp.a
mkfont_OBJS = mkfont/mkfont.o common/assetcomp.a
mkmodel_OBJS = mkmodel/mkmodel.o common/assetcomp.a
n64dso_OBJS = n64dso/n64dso.o common/assetcomp.a
n64dso-extern_OBJS = n64dso/n64dso-extern.o
n64dso-msym_OBJS = n64dso/n64dso-msym.o
audioconv64_OBJS = audioconv64/audioconv64.o
rdpvalidate_OBJS = rdpvalidate/rdpvalidate.o
mkdfs_OBJS = mkdfs/mkdfs.o
dumpdfs_OBJS = dumpdfs/dumpdfs.o
n64tool_OBJS = n64tool.o
n64sym_OBJS = n64sym.o
ed64romconfig_OBJS = ed64romconfig.o
n64elfcompress_OBJS = n64elfcompress/n64elfcompress.o common/assetcomp.a
n64elfcompress/n64elfcompress.o: n64elfcompress/n64elfcompress.c $(DECOMP_STUBS)

TOOLS = n64tool n64sym n64elfcompress ed64romconfig audioconv64 mkdfs dumpdfs mkasset mksprite mkfont mkmodel n64dso n64dso-msym n64dso-extern rdpvalidate

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
	rm -f $$($(1)_BIN) $$($(1)_DIR)/*.o $$($(1)_DIR)/*.d
-include $$(wildcard $$($(1)_DIR)/*.d)
endef

$(foreach tool,$(TOOLS),$(eval $(call TOOL_template,$(tool))))
all: $(TOOLS)
install: $(foreach tool,$(TOOLS),$(tool)-install)
clean: $(foreach tool,$(TOOLS),$(tool)-clean) common-clean
.PHONY: all install clean

ifneq ($(V),1)
.SILENT:
endif
