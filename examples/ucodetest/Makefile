BUILD_DIR = build
include $(N64_INST)/include/n64.mk

.DEFAULT_GOAL := ucodetest.z64

objs = $(BUILD_DIR)/ucodetest.o $(BUILD_DIR)/rsp_basic.o
deps = $(objs:$(BUILD_DIR)/%.o=$(BUILD_DIR)/%.d)

ucodetest.z64: N64_ROM_TITLE="UCode Test"

$(BUILD_DIR)/ucodetest.elf: $(objs)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(deps)
