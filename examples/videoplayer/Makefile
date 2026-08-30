BUILD_DIR=build
# This example uses fmv_play and various preview APIs
LIBDRAGON_PREVIEW = 2
include $(N64_INST)/include/n64.mk

# Set to 1 to load the video codecs from DSOs
VIDEO_CODEC_IN_DSO := 0

MOVIE_FILE=caminandes.ogv
MOVIE_URL=https://archive.org/download/CaminandesLlamigos/Caminandes_%20Llamigos-1080p.ogv
DFS_FILES=filesystem/caminandes.h264 filesystem/caminandes.wav64 filesystem/caminandes.seek

ifeq ($(VIDEO_CODEC_IN_DSO),1)
DSOS := filesystem/video_codec_h264.dso
DFS_FILES += $(DSOS)
CFLAGS += -DVIDEO_CODEC_IN_DSO
endif

src = videoplayer.c

all: videoplayer.z64

videoplayer.z64: N64_ROM_TITLE="Video Player"
videoplayer.z64: $(BUILD_DIR)/videoplayer.dfs

$(BUILD_DIR)/videoplayer.elf: $(src:%.c=$(BUILD_DIR)/%.o)

# If the movie file could not be encoded, build the ROM without it.
$(BUILD_DIR)/videoplayer.dfs: $(DFS_FILES)
	@echo "    [DFS] $@"
	if [ ! -s "$<" ]; then rm -f "$<" ; fi
	$(N64_MKDFS) "$@" filesystem >/dev/null

# Re-encode the movie's video stream if ffmpeg is available
filesystem/caminandes.m1v: $(MOVIE_FILE)
	@mkdir -p "$(dir $@)"
	@echo "    [VIDEO] $@"
	if [ -s "$<" ]; then \
		if command -v ffmpeg >/dev/null; then \
			$(N64_VIDEOCONV) -o filesystem --codec mpeg1 --quality 70 --quick --seek 5 "$<"; \
		else \
			echo "videoplayer: Skipping video re-encoding because ffmpeg is not installed" ; \
			touch "$@" ; \
		fi ; \
	else \
		echo "videoplayer: Skipping video re-encoding because the sample movie is missing" ; \
		touch "$@" ; \
	fi

# Re-encode the movie's video stream if ffmpeg is available
filesystem/caminandes.h264: $(MOVIE_FILE)
	@mkdir -p "$(dir $@)"
	@echo "    [VIDEO] $@"
	if [ -s "$<" ]; then \
		if command -v ffmpeg >/dev/null; then \
			$(N64_VIDEOCONV) -o filesystem --codec h264 --quality 70 --quick --seek 5 "$<"; \
		else \
			echo "videoplayer: Skipping video re-encoding because ffmpeg is not installed" ; \
			touch "$@" ; \
		fi ; \
	else \
		echo "videoplayer: Skipping video re-encoding because the sample movie is missing" ; \
		touch "$@" ; \
	fi

filesystem/caminandes.wav64: filesystem/caminandes.h264
filesystem/caminandes.seek: filesystem/caminandes.h264

ifeq ($(VIDEO_CODEC_IN_DSO),1)
filesystem/video_codec_h264.dso filesystem/video_codec_h264.dso.sym: $(N64_VIDEO_CODEC_H264_DSO) $(N64_VIDEO_CODEC_H264_DSO_SYM)
	@mkdir -p "$(dir $@)"
	@echo "    [CP DSO] $@"
	cp $(N64_VIDEO_CODEC_H264_DSO) filesystem/video_codec_h264.dso
	cp $(N64_VIDEO_CODEC_H264_DSO_SYM) filesystem/video_codec_h264.dso.sym

MAIN_ELF_EXTERNS := $(BUILD_DIR)/videoplayer.externs

$(MAIN_ELF_EXTERNS): $(DSOS)
$(BUILD_DIR)/videoplayer.elf: $(MAIN_ELF_EXTERNS)

videoplayer.z64: $(BUILD_DIR)/videoplayer.msym
$(BUILD_DIR)/videoplayer.msym: $(BUILD_DIR)/videoplayer.elf $(MAIN_ELF_EXTERNS)
endif

# Download the movie file if wget or curl is available and it isn't already downloaded
$(MOVIE_FILE):
	if [ ! -e "$@" ]; then \
		if command -v wget ; then \
			wget -O "$@" "$(MOVIE_URL)"  ; \
		elif command -v curl ; then \
			curl -Lo "$@" "$(MOVIE_URL)" ; \
		else \
			echo "videoplayer: Skipping sample movie download because wget or curl are not installed" ; \
			touch "$@" ; \
		fi ; \
	fi

clean:
	rm -rf $(BUILD_DIR) $(DFS_FILES) videoplayer.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
