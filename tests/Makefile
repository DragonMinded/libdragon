BUILD_DIR=build
USO_PLF_BASE_DIR=$(BUILD_DIR)
USO_BASE_DIR=filesystem
USO_MODULES = dl_test_syms.plf dl_test_relocs.plf dl_test_imports.plf dl_test_ctors.plf
include $(N64_INST)/include/n64.mk

all: testrom.z64 testrom_emu.z64

$(BUILD_DIR)/testrom.dfs: $(wildcard filesystem/*) $(USO_LIST)

OBJS = $(BUILD_DIR)/test_constructors_cpp.o \
	   $(BUILD_DIR)/rsp_test.o \
	   $(BUILD_DIR)/rsp_test2.o \
	   $(BUILD_DIR)/backtrace.o \

$(BUILD_DIR)/testrom.elf: $(BUILD_DIR)/testrom.o $(OBJS)
testrom.z64: N64_ROM_TITLE="Libdragon Test ROM"
testrom.z64: $(BUILD_DIR)/testrom.dfs

$(BUILD_DIR)/testrom_emu.elf: $(BUILD_DIR)/testrom_emu.o $(OBJS)
testrom_emu.z64: N64_ROM_TITLE="Libdragon Test ROM"
testrom_emu.z64: $(BUILD_DIR)/testrom.dfs

$(BUILD_DIR)/testrom_emu.o: $(SOURCE_DIR)/testrom.c
	@mkdir -p $(dir $@)
	@echo "    [CC] $<"
	$(CC) -c $(CFLAGS) -DIN_EMULATOR=1 -o $@ $<

${BUILD_DIR}/rsp_test.o: IS_OVERLAY=1

$(USO_PLF_BASE_DIR)/dl_test_syms.plf: $(BUILD_DIR)/dl_test_syms.o
$(USO_PLF_BASE_DIR)/dl_test_relocs.plf: $(BUILD_DIR)/dl_test_relocs.o
$(USO_PLF_BASE_DIR)/dl_test_imports.plf: $(BUILD_DIR)/dl_test_imports.o
$(USO_PLF_BASE_DIR)/dl_test_ctors.plf: $(BUILD_DIR)/dl_test_ctors.o

clean:
	rm -rf $(BUILD_DIR) testrom.z64 testrom_emu.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
