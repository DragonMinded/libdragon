BUILD_DIR=build
USO_ELF_DIR=$(BUILD_DIR)
include $(N64_INST)/include/n64.mk

all: testrom.z64 testrom_emu.z64

MAIN_ELF_EXTERNS := $(BUILD_DIR)/testrom.externs
USO_MODULES = dl_test_syms.uso dl_test_relocs.uso dl_test_imports.uso dl_test_ctors.uso
USO_LIST = $(addprefix filesystem/, $(USO_MODULES))

$(BUILD_DIR)/testrom.dfs: $(wildcard filesystem/*) $(USO_LIST)

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

$(MAIN_ELF_EXTERNS): $(USO_LIST)
filesystem/dl_test_syms.uso: $(BUILD_DIR)/dl_test_syms.o
filesystem/dl_test_relocs.uso: $(BUILD_DIR)/dl_test_relocs.o
filesystem/dl_test_imports.uso: $(BUILD_DIR)/dl_test_imports.o
filesystem/dl_test_ctors.uso: $(BUILD_DIR)/dl_test_ctors.o

clean:
	rm -rf $(BUILD_DIR) testrom.z64 testrom_emu.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
