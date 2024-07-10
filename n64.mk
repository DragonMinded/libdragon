BUILD_DIR ?= .
SOURCE_DIR ?= .
DSO_COMPRESS_LEVEL ?= 1

N64_ROM_TITLE = "Made with libdragon" # Override this with the name of your game or project
N64_ROM_SAVETYPE = # Supported savetypes: none eeprom4k eeprom16 sram256k sram768k sram1m flashram
N64_ROM_RTC = # Set to true to enable the Joybus Real-Time Clock
N64_ROM_REGIONFREE = # Set to true to allow booting on any console region
N64_ROM_REGION = # Set to a region code (emulators will boot on a specific console region)
N64_ROM_ELFCOMPRESS = 1 # Set compression level of ELF file in ROM
N64_ROM_CONTROLLER1 = # Sets the type of Controller 1 in the Advanced Homebrew Header. This could influence emulator behaviour such as Ares'
N64_ROM_CONTROLLER2 = # Sets the type of Controller 2 in the Advanced Homebrew Header. This could influence emulator behaviour such as Ares'
N64_ROM_CONTROLLER3 = # Sets the type of Controller 3 in the Advanced Homebrew Header. This could influence emulator behaviour such as Ares'
N64_ROM_CONTROLLER4 = # Sets the type of Controller 4 in the Advanced Homebrew Header. This could influence emulator behaviour such as Ares'

# Override this to use a toolchain installed separately from libdragon
N64_GCCPREFIX ?= $(N64_INST)
N64_ROOTDIR = $(N64_INST)
N64_BINDIR = $(N64_ROOTDIR)/bin
N64_INCLUDEDIR = $(N64_ROOTDIR)/mips64-elf/include
N64_LIBDIR = $(N64_ROOTDIR)/mips64-elf/lib
N64_GCCPREFIX_TRIPLET = $(N64_GCCPREFIX)/bin/mips64-elf-

COMMA:=,

N64_CC = $(N64_GCCPREFIX_TRIPLET)gcc
N64_CXX = $(N64_GCCPREFIX_TRIPLET)g++
N64_AS = $(N64_GCCPREFIX_TRIPLET)as
N64_AR = $(N64_GCCPREFIX_TRIPLET)ar
N64_LD = $(N64_GCCPREFIX_TRIPLET)ld
N64_OBJCOPY = $(N64_GCCPREFIX_TRIPLET)objcopy
N64_OBJDUMP = $(N64_GCCPREFIX_TRIPLET)objdump
N64_SIZE = $(N64_GCCPREFIX_TRIPLET)size
N64_NM = $(N64_GCCPREFIX_TRIPLET)nm
N64_STRIP = $(N64_GCCPREFIX_TRIPLET)strip

N64_ED64ROMCONFIG = $(N64_BINDIR)/ed64romconfig
N64_MKDFS = $(N64_BINDIR)/mkdfs
N64_TOOL = $(N64_BINDIR)/n64tool
N64_SYM = $(N64_BINDIR)/n64sym
N64_ELFCOMPRESS = $(N64_BINDIR)/n64elfcompress
N64_AUDIOCONV = $(N64_BINDIR)/audioconv64
N64_MKSPRITE = $(N64_BINDIR)/mksprite
N64_MKFONT = $(N64_BINDIR)/mkfont
N64_MKMODEL = $(N64_BINDIR)/mkmodel
N64_DSO = $(N64_BINDIR)/n64dso
N64_DSOEXTERN = $(N64_BINDIR)/n64dso-extern
N64_DSOMSYM = $(N64_BINDIR)/n64dso-msym

N64_C_AND_CXX_FLAGS =  -march=vr4300 -mtune=vr4300 -I$(N64_INCLUDEDIR)
N64_C_AND_CXX_FLAGS += -falign-functions=32   # NOTE: if you change this, also change backtrace() in backtrace.c
N64_C_AND_CXX_FLAGS += -ffunction-sections -fdata-sections -g -ffile-prefix-map="$(CURDIR)"=
N64_C_AND_CXX_FLAGS += -ffast-math -ftrapping-math -fno-associative-math
N64_C_AND_CXX_FLAGS += -DN64 -O2 -Wall -Werror -Wno-error=deprecated-declarations -fdiagnostics-color=always
N64_C_AND_CXX_FLAGS += -Wno-error=unused-variable -Wno-error=unused-but-set-variable -Wno-error=unused-function -Wno-error=unused-parameter -Wno-error=unused-but-set-parameter -Wno-error=unused-label -Wno-error=unused-local-typedefs -Wno-error=unused-const-variable
N64_CFLAGS = $(N64_C_AND_CXX_FLAGS) -std=gnu17
N64_CXXFLAGS = $(N64_C_AND_CXX_FLAGS) -std=gnu++17
N64_ASFLAGS = -mtune=vr4300 -march=vr4300 -Wa,--fatal-warnings -I$(N64_INCLUDEDIR)
N64_RSPASFLAGS = -march=mips1 -mabi=32 -Wa,--fatal-warnings -I$(N64_INCLUDEDIR)
N64_LDFLAGS = -g -L$(N64_LIBDIR) -ldragon -lm -ldragonsys -Tn64.ld --gc-sections --wrap __do_global_ctors
N64_DSOLDFLAGS = --emit-relocs --unresolved-symbols=ignore-all --nmagic -T$(N64_LIBDIR)/dso.ld

N64_TOOLFLAGS = --title $(N64_ROM_TITLE)
N64_TOOLFLAGS += $(if $(N64_ROM_HEADER),--header $(N64_ROM_HEADER))
N64_TOOLFLAGS += $(if $(N64_ROM_REGION),--region $(N64_ROM_REGION))
N64_ED64ROMCONFIGFLAGS =  $(if $(N64_ROM_SAVETYPE),--savetype $(N64_ROM_SAVETYPE))
N64_ED64ROMCONFIGFLAGS += $(if $(N64_ROM_RTC),--rtc) 
N64_ED64ROMCONFIGFLAGS += $(if $(N64_ROM_REGIONFREE),--regionfree)
N64_ED64ROMCONFIGFLAGS += $(if $(N64_ROM_CONTROLLER_TYPE1),--controller1 $(N64_ROM_CONTROLLER_TYPE1))
N64_ED64ROMCONFIGFLAGS += $(if $(N64_ROM_CONTROLLER_TYPE2),--controller2 $(N64_ROM_CONTROLLER_TYPE2))
N64_ED64ROMCONFIGFLAGS += $(if $(N64_ROM_CONTROLLER_TYPE3),--controller3 $(N64_ROM_CONTROLLER_TYPE3))
N64_ED64ROMCONFIGFLAGS += $(if $(N64_ROM_CONTROLLER_TYPE4),--controller4 $(N64_ROM_CONTROLLER_TYPE4))

ifeq ($(D),1)
CFLAGS+=-g3
CXXFLAGS+=-g3
ASFLAGS+=-g
RSPASFLAGS+=-g
LDFLAGS+=-g
endif

# automatic .d dependency generation
CFLAGS+=-MMD     
CXXFLAGS+=-MMD
ASFLAGS+=-MMD
RSPASFLAGS+=-MMD

# Change all the dependency chain of z64 ROMs to use the N64 toolchain.
%.z64: CC=$(N64_CC)
%.z64: CXX=$(N64_CXX)
%.z64: AS=$(N64_AS)
%.z64: LD=$(N64_LD)
%.z64: CFLAGS+=$(N64_CFLAGS)
%.z64: CXXFLAGS+=$(N64_CXXFLAGS)
%.z64: ASFLAGS+=$(N64_ASFLAGS)
%.z64: RSPASFLAGS+=$(N64_RSPASFLAGS)
%.z64: LDFLAGS+=$(N64_LDFLAGS)
%.z64: $(BUILD_DIR)/%.elf
	@echo "    [Z64] $@"
	$(N64_SYM) $< $<.sym
	cp $< $<.stripped
	$(N64_STRIP) -s $<.stripped
	$(N64_ELFCOMPRESS) -o $(dir $<) -c $(N64_ROM_ELFCOMPRESS) $<.stripped
	@rm -f $@
	DFS_FILE="$(filter %.dfs, $^)"; \
	if [ -z "$$DFS_FILE" ]; then \
		$(N64_TOOL) $(N64_TOOLFLAGS) --toc --output $@ --align 256 $<.stripped --align 8 $<.sym --align 8; \
	else \
		MSYM_FILE="$(filter %.msym, $^)"; \
		if [ -z "$$MSYM_FILE" ]; then \
			$(N64_TOOL) $(N64_TOOLFLAGS) --toc --output $@ --align 256 $<.stripped --align 8 $<.sym --align 16 "$$DFS_FILE"; \
		else \
			$(N64_TOOL) $(N64_TOOLFLAGS) --toc --output $@ --align 256 $<.stripped --align 8 $<.sym --align 8 "$$MSYM_FILE" --align 16 "$$DFS_FILE"; \
		fi \
	fi
	if [ ! -z "$(strip $(N64_ED64ROMCONFIGFLAGS))" ]; then \
		$(N64_ED64ROMCONFIG) $(N64_ED64ROMCONFIGFLAGS) $@; \
	fi

%.v64: %.z64
	@echo "    [V64] $@"
	$(N64_OBJCOPY) -I binary -O binary --reverse-bytes=2 $< $@

%.dfs:
	@mkdir -p $(dir $@)
	@echo "    [DFS] $@"
	$(N64_MKDFS) $@ $(<D) >/dev/null

# Assembly rule. We use .S for both RSP and MIPS assembly code, and we differentiate
# using the prefix of the filename: if it starts with "rsp", it is RSP ucode, otherwise
# it's a standard MIPS assembly file.
$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.S
	@mkdir -p $(dir $@)
	set -e; \
	FILENAME="$(notdir $(basename $@))"; \
	if case "$$FILENAME" in "rsp"*) true;; *) false;; esac; then \
		SYMPREFIX="$(subst .,_,$(subst /,_,$(basename $@)))"; \
		TEXTSECTION="$(basename $@).text"; \
		DATASECTION="$(basename $@).data"; \
		BINARY="$(basename $@).elf"; \
		echo "    [RSP] $<"; \
		$(N64_CC) $(RSPASFLAGS) -L$(N64_LIBDIR) -nostartfiles -Wl,-Trsp.ld -Wl,--gc-sections  -Wl,-Map=$(BUILD_DIR)/$(notdir $(basename $@)).map -o $@ $<; \
		mv "$@" $$BINARY; \
		$(N64_OBJCOPY) -O binary -j .text $$BINARY $$TEXTSECTION.bin; \
		$(N64_OBJCOPY) -O binary -j .data $$BINARY $$DATASECTION.bin; \
		$(N64_OBJCOPY) -I binary -O elf32-bigmips -B mips4300 \
				--redefine-sym _binary_$${SYMPREFIX}_text_bin_start=$${FILENAME}_text_start \
				--redefine-sym _binary_$${SYMPREFIX}_text_bin_end=$${FILENAME}_text_end \
				--redefine-sym _binary_$${SYMPREFIX}_text_bin_size=$${FILENAME}_text_size \
				--set-section-alignment .data=16 \
				--rename-section .text=.data $$TEXTSECTION.bin $$TEXTSECTION.o; \
		$(N64_OBJCOPY) -I binary -O elf32-bigmips -B mips4300 \
				--redefine-sym _binary_$${SYMPREFIX}_data_bin_start=$${FILENAME}_data_start \
				--redefine-sym _binary_$${SYMPREFIX}_data_bin_end=$${FILENAME}_data_end \
				--redefine-sym _binary_$${SYMPREFIX}_data_bin_size=$${FILENAME}_data_size \
				--set-section-alignment .data=16 \
				--rename-section .text=.data $$DATASECTION.bin $$DATASECTION.o; \
		$(N64_SIZE) -G $$BINARY; \
		$(N64_LD) -relocatable $$TEXTSECTION.o $$DATASECTION.o -o $@; \
		rm $$TEXTSECTION.bin $$DATASECTION.bin $$TEXTSECTION.o $$DATASECTION.o; \
	else \
		echo "    [AS] $<"; \
		$(CC) -c $(ASFLAGS) -o $@ $<; \
	fi

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c 
	@mkdir -p $(dir $@)
	@echo "    [CC] $<"
	$(CC) -c $(CFLAGS) -o $@ $<

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "    [CXX] $<"
	$(CXX) -c $(CXXFLAGS) -o $@ $<

%.elf: $(N64_LIBDIR)/libdragon.a $(N64_LIBDIR)/libdragonsys.a $(N64_LIBDIR)/n64.ld
	@mkdir -p $(dir $@)
	@echo "    [LD] $@"
# We always use g++ to link except for ucode and DSO files because of the inconsistencies
# between ld when it comes to global ctors dtors. Also see __do_global_ctors
	EXTERNS_FILE="$(filter %.externs, $^)"; \
	if [ -z "$$EXTERNS_FILE" ]; then \
		$(CXX) -o $@ $(filter %.o, $^) $(filter-out $(N64_LIBDIR)/libdragon.a $(N64_LIBDIR)/libdragonsys.a, $(filter %.a, $^)) \
			-lc $(patsubst %,-Wl$(COMMA)%,$(LDFLAGS)) -Wl,-Map=$(BUILD_DIR)/$(notdir $(basename $@)).map; \
	else \
		$(CXX) -o $@ $(filter %.o, $^) $(filter-out $(N64_LIBDIR)/libdragon.a $(N64_LIBDIR)/libdragonsys.a, $(filter %.a, $^)) \
			-lc $(patsubst %,-Wl$(COMMA)%,$(LDFLAGS)) -Wl,-T"$$EXTERNS_FILE" -Wl,-Map=$(BUILD_DIR)/$(notdir $(basename $@)).map; \
	fi
	$(N64_SIZE) -G $@

	
# Change all the dependency chain of DSO files to use the N64 toolchain.
%.dso: CC=$(N64_CC)
%.dso: CXX=$(N64_CXX)
%.dso: AS=$(N64_AS)
%.dso: LD=$(N64_LD)
%.dso: CFLAGS+=$(N64_CFLAGS) -mno-gpopt $(DSO_CFLAGS)
%.dso: CXXFLAGS+=$(N64_CXXFLAGS) -mno-gpopt $(DSO_CXXFLAGS)
%.dso: ASFLAGS+=$(N64_ASFLAGS)
%.dso: RSPASFLAGS+=$(N64_RSPASFLAGS)

%.dso: $(N64_LIBDIR)/dso.ld
	$(eval DSO_ELF=$(basename $(BUILD_DIR)/dso_elf/$@).elf)
	@mkdir -p $(dir $@)
	@mkdir -p $(dir $(DSO_ELF))
	@echo "    [DSO] $@"
	$(N64_LD) $(N64_DSOLDFLAGS) -Map=$(basename $(DSO_ELF)).map -o $(DSO_ELF) $(filter %.o, $^)
	$(N64_SIZE) -G $(DSO_ELF)
	$(N64_DSO) -o $(dir $@) -c $(DSO_COMPRESS_LEVEL) $(DSO_ELF)
	$(N64_SYM) $(DSO_ELF) $@.sym
	
%.externs:
	@echo "    [DSOEXTERN] $@"
	$(N64_DSOEXTERN) -o $@ $^ 
	
%.msym: %.elf
	@echo "    [MSYM] $@"
	$(N64_DSOMSYM) $< $@
    
ifneq ($(V),1)
.SILENT:
endif
