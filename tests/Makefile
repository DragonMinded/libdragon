BUILD_DIR=build
include $(N64_INST)/include/n64.mk

all: testrom.z64 testrom_emu.z64

$(BUILD_DIR)/testrom.dfs: $(wildcard filesystem/*)

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

clean:
	rm -rf $(BUILD_DIR) testrom.z64 testrom_emu.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
