LIBDRAGON_OBJS += \
	$(BUILD_DIR)/video/video.o \
	$(BUILD_DIR)/video/video_sync.o \
	$(BUILD_DIR)/video/fmv.o \
	$(BUILD_DIR)/video/mpeg1.o \
	$(BUILD_DIR)/video/yuv.o \
	$(BUILD_DIR)/video/rsp_yuv.o \
	$(BUILD_DIR)/video/rsp_mpeg1.o \
	$(BUILD_DIR)/video/h264_decoder.o \
	$(BUILD_DIR)/video/h264.o \
	$(BUILD_DIR)/video/rsph264_inter.o \
	$(BUILD_DIR)/video/rsph264_intra.o \
	$(BUILD_DIR)/video/subtitles.o

LIBDRAGON_DSOS += $(BUILD_DIR)/dso/video_codec_h264.dso
$(BUILD_DIR)/dso/video_codec_h264.dso: \
	$(BUILD_DIR)/dso/video/h264_decoder.o \
	$(BUILD_DIR)/dso/video/h264.o \
	$(BUILD_DIR)/video/rsph264_inter.o \
	$(BUILD_DIR)/video/rsph264_intra.o \
	$(BUILD_DIR)/dso/video/h264_dso.o
$(BUILD_DIR)/dso/video_codec_h264.dso: N64_DSOLDFLAGS += --gc-sections --gc-keep-exported
