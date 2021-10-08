BUILD_DIR=build
SOURCE_DIR=.
include $(N64_INST)/include/n64.mk

cpptest.z64: MAPCMD=-Wl,-Map
cpptest.z64: LD=$(N64_CXX)
cpptest.z64: LDFLAGS=-L$(N64_ROOTDIR)/mips64-elf/lib -ldragon -lc -lm -ldragonsys -Wl,-Tn64.ld -Wl,--gc-sections

all: cpptest.z64

$(BUILD_DIR)/cpptest.elf: \
	$(BUILD_DIR)/cpptest.o

cpptest.z64: N64_ROM_TITLE="cpptest"

clean:
	rm -rf $(BUILD_DIR) cpptest.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
