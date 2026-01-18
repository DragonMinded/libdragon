BUILD_DIR=build
DSO_COMPRESS_LEVEL=0
include $(N64_INST)/include/n64.mk

N64_MKMATERIAL = $(N64_INST)/bin/mkmaterial

all: testrom.z64 testrom_emu.z64 testrom_h264.z64

MAIN_ELF_EXTERNS := $(BUILD_DIR)/testrom.externs
DSO_MODULES = dl_test_syms.dso dl_test_relocs.dso dl_test_imports.dso dl_test_ctors.dso
DSO_LIST = $(addprefix filesystem/, $(DSO_MODULES))

$(BUILD_DIR)/testrom.dfs: $(wildcard filesystem/*) $(DSO_LIST) filesystem/test_dir

ASSETS = filesystem/grass1.ci8.sprite \
		 filesystem/grass1.rgba32.sprite \
		 filesystem/grass1sq.rgba32.sprite \
		 filesystem/grass2.rgba32.sprite \
		 filesystem/materials.mdb

OBJS = $(BUILD_DIR)/test_constructors_cpp.o \
	   $(BUILD_DIR)/rsp_test.o \
	   $(BUILD_DIR)/rsp_test2.o \
	   $(BUILD_DIR)/backtrace.o \

filesystem/grass1sq.rgba32.sprite: MKSPRITE_FLAGS=--texparms 0,0,2,0
filesystem/grass2.rgba32.sprite: MKSPRITE_FLAGS=--mipmap BOX

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) $(MKSPRITE_FLAGS) -o filesystem "$<"

filesystem/materials.mdb: $(wildcard assets/*.mat)
	@mkdir -p $(dir $@)
	@echo "    [MDB] $@"
	$(N64_MKMATERIAL) -o filesystem "$^"

filesystem/test_dir:
	@mkdir -p $(dir $@)
	@echo "    [DIR] $@"
	mkdir -p filesystem/aaaa-empty1
	mkdir -p filesystem/aaaa-empty2
	mkdir -p filesystem/test_dir/subdir
	mkdir -p filesystem/test_dir/subdir/subempty1
	mkdir -p filesystem/test_dir/subdir/subempty2
	mkdir -p filesystem/test_dir/subdir/subsubdir
	touch filesystem/test_dir/subdir/subsubdir/file3.txt
	touch filesystem/test_dir/subdir/file2.txt
	touch filesystem/test_dir/file1.txt


$(BUILD_DIR)/testrom.elf: $(BUILD_DIR)/testrom.o $(OBJS) $(MAIN_ELF_EXTERNS) $(ASSETS)
testrom.z64: N64_ROM_TITLE="Libdragon Test ROM"
testrom.z64: $(BUILD_DIR)/testrom.dfs $(BUILD_DIR)/testrom.msym

$(BUILD_DIR)/testrom_emu.elf: $(BUILD_DIR)/testrom_emu.o $(OBJS) $(MAIN_ELF_EXTERNS) $(ASSETS)
testrom_emu.z64: N64_ROM_TITLE="Libdragon Test ROM"
testrom_emu.z64: $(BUILD_DIR)/testrom.dfs $(BUILD_DIR)/testrom_emu.msym

$(BUILD_DIR)/testrom_h264.elf: $(BUILD_DIR)/test_h264.o
testrom_emu.z64: N64_ROM_TITLE="Libdragon H264 Test"

$(BUILD_DIR)/testrom_emu.o: $(SOURCE_DIR)/testrom.c
	@mkdir -p $(dir $@)
	@echo "    [CC] $<"
	$(CC) -c $(CFLAGS) -DIN_EMULATOR=1 -o $@ $<

${BUILD_DIR}/rsp_test.o: IS_OVERLAY=1

$(BUILD_DIR)/testrom_emu.msym: $(BUILD_DIR)/testrom_emu.elf
$(BUILD_DIR)/testrom.msym: $(BUILD_DIR)/testrom.elf

$(MAIN_ELF_EXTERNS): $(DSO_LIST)
filesystem/dl_test_syms.dso: $(BUILD_DIR)/dl_test_syms.o
filesystem/dl_test_relocs.dso: $(BUILD_DIR)/dl_test_relocs.o
filesystem/dl_test_imports.dso: $(BUILD_DIR)/dl_test_imports.o
filesystem/dl_test_ctors.dso: $(BUILD_DIR)/dl_test_ctors.o

clean:
	rm -rf $(BUILD_DIR) testrom.z64 testrom_emu.z64 testrom_h264.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
