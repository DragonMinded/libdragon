BUILD_DIR=build
include $(N64_INST)/include/n64.mk

all: testrom.z64 testrom_emu.z64

MAIN_ELF_EXTERNS := $(BUILD_DIR)/testrom.externs
DSO_MODULES = dl_test_syms.dso dl_test_relocs.dso dl_test_imports.dso dl_test_ctors.dso
DSO_LIST = $(addprefix filesystem/, $(USO_MODULES))

$(BUILD_DIR)/testrom.dfs: $(wildcard filesystem/*) $(DSO_LIST)

OBJS = $(BUILD_DIR)/test_constructors_cpp.o \
	   $(BUILD_DIR)/rsp_test.o \
	   $(BUILD_DIR)/rsp_test2.o \
	   $(BUILD_DIR)/backtrace.o \

$(BUILD_DIR)/testrom.elf: $(BUILD_DIR)/testrom.o $(OBJS) $(MAIN_ELF_EXTERNS)
testrom.z64: N64_ROM_TITLE="Libdragon Test ROM"
testrom.z64: $(BUILD_DIR)/testrom.dfs

$(BUILD_DIR)/testrom_emu.elf: $(BUILD_DIR)/testrom_emu.o $(OBJS) $(MAIN_ELF_EXTERNS)
testrom_emu.z64: N64_ROM_TITLE="Libdragon Test ROM"
testrom_emu.z64: $(BUILD_DIR)/testrom.dfs

$(BUILD_DIR)/testrom_emu.o: $(SOURCE_DIR)/testrom.c
	@mkdir -p $(dir $@)
	@echo "    [CC] $<"
	$(CC) -c $(CFLAGS) -DIN_EMULATOR=1 -o $@ $<

${BUILD_DIR}/rsp_test.o: IS_OVERLAY=1

$(MAIN_ELF_EXTERNS): $(DSO_LIST)
filesystem/dl_test_syms.dso: $(BUILD_DIR)/dl_test_syms.o
filesystem/dl_test_relocs.dso: $(BUILD_DIR)/dl_test_relocs.o
filesystem/dl_test_imports.dso: $(BUILD_DIR)/dl_test_imports.o
filesystem/dl_test_ctors.dso: $(BUILD_DIR)/dl_test_ctors.o

clean:
	rm -rf $(BUILD_DIR) testrom.z64 testrom_emu.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
