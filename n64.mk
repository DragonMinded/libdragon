BUILD_DIR ?= .
SOURCE_DIR ?= .

N64_ROOTDIR = $(N64_INST)
N64_GCCPREFIX = $(N64_ROOTDIR)/bin/mips64-elf-
N64_CHKSUMPATH = $(N64_ROOTDIR)/bin/chksum64
N64_MKDFSPATH = $(N64_ROOTDIR)/bin/mkdfs
N64_HEADERPATH = $(N64_ROOTDIR)/mips64-elf/lib
N64_TOOL = $(N64_ROOTDIR)/bin/n64tool
N64_HEADERNAME = header

N64_CFLAGS = -DN64 -falign-functions=32 -ffunction-sections -fdata-sections -std=gnu99 -march=vr4300 -mtune=vr4300 -O2 -Wall -Werror -fdiagnostics-color=always -I$(ROOTDIR)/mips64-elf/include
N64_ASFLAGS = -mtune=vr4300 -march=vr4300 -Wa,--fatal-warnings
N64_LDFLAGS = -L$(N64_ROOTDIR)/mips64-elf/lib -ldragon -lc -lm -ldragonsys -Tn64.ld --gc-sections

N64_CC = $(N64_GCCPREFIX)gcc
N64_AS = $(N64_GCCPREFIX)as
N64_LD = $(N64_GCCPREFIX)ld
N64_OBJCOPY = $(N64_GCCPREFIX)objcopy
N64_OBJDUMP = $(N64_GCCPREFIX)objdump
N64_SIZE = $(N64_GCCPREFIX)size

N64_ROM_TITLE = "N64 ROM"

ifeq ($(D),1)
CFLAGS+=-g3
ASFLAGS+=-g
LDFLAGS+=-g
endif

N64_FLAGS = -h $(N64_HEADERPATH)/$(N64_HEADERNAME)

CFLAGS+=-MMD     # automatic .d dependency generation
ASFLAGS+=-MMD    # automatic .d dependency generation

# Change all the dependency chain of z64 ROMs to use the N64 toolchain.
%.z64: CC=$(N64_CC)
%.z64: AS=$(N64_AS)
%.z64: LD=$(N64_LD)
%.z64: CFLAGS+=$(N64_CFLAGS)
%.z64: ASFLAGS+=$(N64_ASFLAGS)
%.z64: LDFLAGS+=$(N64_LDFLAGS)
%.z64: $(BUILD_DIR)/%.elf
	@echo "    [N64] $@"
	$(N64_OBJCOPY) $< $<.bin -O binary
	@rm -f $@
	DFS_FILE=$(filter %.dfs, $^); \
	if [ -z "$$DFS_FILE" ]; then \
		$(N64_TOOL) $(N64_FLAGS) -o $@  -t $(N64_ROM_TITLE) $<.bin; \
	else \
		$(N64_TOOL) $(N64_FLAGS) -o $@  -t $(N64_ROM_TITLE) $<.bin -s 1M $$DFS_FILE; \
	fi
	$(N64_CHKSUMPATH) $@ >/dev/null

# Support v64 ROMs via dd byteswap
ifeq ($(N64_BYTE_SWAP),true)
%.v64: %.z64
	dd conv=swab if=$^ of=$@
endif

%.dfs:
	@mkdir -p $(dir $@)
	@echo "    [DFS] $@"
	$(N64_MKDFSPATH) $@ $(<D) >/dev/null

# Assembly rule. We use .S for both RSP and MIPS assembly code, and we differentiate
# using the prefix of the filename: if it starts with "rsp", it is RSP ucode, otherwise
# it's a standard MIPS assembly file.
$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.S
	@mkdir -p $(dir $@)
	set -e; if case $(notdir $(basename $@)) in "rsp"*) true;; *) false;; esac; then \
		echo "    [RSP] $<"; \
		$(N64_CC) $(N64_ASFLAGS) -nostartfiles -MMD -Wl,-Ttext=0x1000 -Wl,-Tdata=0x0 -Wl,-e0x1000 -o $@ $<; \
		$(N64_OBJCOPY) -O binary -j .text $@ $(basename $@).text.bin; \
		$(N64_OBJCOPY) -O binary -j .data $@ $(basename $@).data.bin; \
		$(N64_OBJCOPY) -I binary -O elf32-bigmips -B mips4300 \
				--redefine-sym _binary_$(subst /,_,$(basename $@))_text_bin_start=$(notdir $(basename $@))_text_start \
				--redefine-sym _binary_$(subst /,_,$(basename $@))_text_bin_end=$(notdir $(basename $@))_text_end \
				--redefine-sym _binary_$(subst /,_,$(basename $@))_text_bin_size=$(notdir $(basename $@))_text_size \
				--set-section-alignment .data=8 \
				--rename-section .text=.data $(basename $@).text.bin $(basename $@).text.o; \
		$(N64_OBJCOPY) -I binary -O elf32-bigmips -B mips4300 \
				--redefine-sym _binary_$(subst /,_,$(basename $@))_data_bin_start=$(notdir $(basename $@))_data_start \
				--redefine-sym _binary_$(subst /,_,$(basename $@))_data_bin_end=$(notdir $(basename $@))_data_end \
				--redefine-sym _binary_$(subst /,_,$(basename $@))_data_bin_size=$(notdir $(basename $@))_data_size \
				--set-section-alignment .data=8 \
				--rename-section .text=.data $(basename $@).data.bin $(basename $@).data.o; \
		$(N64_LD) -relocatable $(basename $@).text.o $(basename $@).data.o -o $@; \
		rm $(basename $@).text.bin $(basename $@).data.bin $(basename $@).text.o $(basename $@).data.o; \
	else \
		echo "    [AS] $<"; \
		$(CC) -c $(ASFLAGS) -o $@ $<; \
	fi

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c 
	@mkdir -p $(dir $@)
	@echo "    [CC] $<"
	$(CC) -c $(CFLAGS) -o $@ $<

%.elf: $(N64_ROOTDIR)/mips64-elf/lib/libdragon.a $(N64_ROOTDIR)/mips64-elf/lib/libdragonsys.a $(N64_ROOTDIR)/mips64-elf/lib/n64.ld
	@mkdir -p $(BUILD_DIR)
	@echo "    [LD] $@"
	$(LD) -o $@ $(filter-out $(N64_ROOTDIR)/mips64-elf/lib/n64.ld,$^) $(LDFLAGS) -Map=$(BUILD_DIR)/$(notdir $(basename $@)).map
	@echo "    [STATS] $@"
	$(N64_SIZE) -G $@

ifneq ($(V),1)
.SILENT:
endif
