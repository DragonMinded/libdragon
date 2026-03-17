BUILD_DIR := build
TARGET := h264decoder_host

ROOT := ../..
ROOT_ABS := $(abspath $(ROOT))
DECODER_DIR := $(ROOT_ABS)/src/video/h264_decoder
OMXDL_DIR := $(DECODER_DIR)/omxdl
LODEPNG_DIR := $(ROOT_ABS)/tools/common

CXX := c++
CC := cc

CXXFLAGS := -O2 -g -std=c++17 -Wall -Wextra -I$(DECODER_DIR) -I$(OMXDL_DIR) -I$(LODEPNG_DIR)
CFLAGS := -O2 -g -std=c11 -Wall -Wextra -I$(DECODER_DIR) -I$(OMXDL_DIR) -I$(LODEPNG_DIR) -D_ERROR_PRINT
LDFLAGS :=
LDLIBS := -lm

CPP_SRCS := h264decoder_host.cpp
CPP_OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(CPP_SRCS))

DECODER_SRCS := $(filter-out $(DECODER_DIR)/h264_decoder.c,$(wildcard $(DECODER_DIR)/*.c))
OMXDL_SRCS := $(wildcard $(OMXDL_DIR)/*.c)
EXTRA_C_SRCS := $(LODEPNG_DIR)/lodepng.c
C_SRCS := $(DECODER_SRCS) $(OMXDL_SRCS) $(EXTRA_C_SRCS)
C_OBJS := $(patsubst $(ROOT_ABS)/%,$(BUILD_DIR)/%,$(C_SRCS:.c=.o))

all: $(TARGET)

$(TARGET): $(CPP_OBJS) $(C_OBJS)
	@mkdir -p "$(dir $@)"
	@echo "    [LD] $@"
	$(CXX) $(LDFLAGS) -o "$@" $^ $(LDLIBS)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p "$(dir $@)"
	@echo "    [CXX] $<"
	$(CXX) $(CXXFLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/%.o: $(ROOT_ABS)/%.c
	@mkdir -p "$(dir $@)"
	@echo "    [CC] $<"
	$(CC) $(CFLAGS) -c "$<" -o "$@"

clean:
	@echo "    [CLEAN] $(BUILD_DIR)"
	rm -rf "$(BUILD_DIR)"

ifneq ($(V),1)
.SILENT:
endif

.PHONY: all clean
