all: rtctest.z64
.PHONY: all

BUILD_DIR = build
include $(N64_INST)/include/n64.mk

SRC = rtctest.c
OBJS = $(SRC:%.c=$(BUILD_DIR)/%.o)
DEPS = $(SRC:%.c=$(BUILD_DIR)/%.d)

rtctest.z64: N64_ROM_TITLE = "RTC Test"
rtctest.z64: N64_ROM_RTC = true

$(BUILD_DIR)/rtctest.elf: $(OBJS)

clean:
	rm -rf $(BUILD_DIR) *.z64
.PHONY: clean

-include $(DEPS)
