BUILD_DIR=build
include n64.mk

all: ucodetest.z64

ucodetest.z64: N64_ROM_TITLE="UCode Test"
ucodetest.elf: $(BUILD_DIR)/ucodetest.o \
               $(BUILD_DIR)/rsp_basic.o

clean:
	rm -f $(BUILD_DIR)/*.d $(BUILD_DIR)/*.o $(BUILD_DIR)/*.map ucodetest.dfs ucodetest.elf ucodetest.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
