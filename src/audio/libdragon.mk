LIBDRAGON_OBJS += \
	$(BUILD_DIR)/audio/mixer.o $(BUILD_DIR)/audio/samplebuffer.o \
	$(BUILD_DIR)/audio/rsp_mixer.o $(BUILD_DIR)/audio/wav64.o \
	$(BUILD_DIR)/audio/xm64.o $(BUILD_DIR)/audio/libxm/play.o \
	$(BUILD_DIR)/audio/libxm/context.o $(BUILD_DIR)/audio/libxm/load.o \
	$(BUILD_DIR)/audio/ym64.o $(BUILD_DIR)/audio/ay8910.o \

LIBDRAGON_OBJS += \
	$(BUILD_DIR)/audio/wav64_opus.o \
	$(BUILD_DIR)/audio/libopus.o \
	$(BUILD_DIR)/audio/libopus_rsp.o \
	$(BUILD_DIR)/audio/rsp_opus_dsp.o \
	$(BUILD_DIR)/audio/rsp_opus_imdct.o \
	$(BUILD_DIR)/audio/rsp_opus_fft_prerot.o \
	$(BUILD_DIR)/audio/rsp_opus_fft_bfly2.o \
	$(BUILD_DIR)/audio/rsp_opus_fft_bfly3.o \
	$(BUILD_DIR)/audio/rsp_opus_fft_bfly4m1.o \
	$(BUILD_DIR)/audio/rsp_opus_fft_bfly4.o \
	$(BUILD_DIR)/audio/rsp_opus_fft_bfly5.o \
	$(BUILD_DIR)/audio/rsp_opus_fft_postrot.o

$(BUILD_DIR)/audio/libopus.o: CFLAGS+=-Wno-all
