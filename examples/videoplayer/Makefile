BUILD_DIR=build
include $(N64_INST)/include/n64.mk

MOVIE_FILE=caminandes.ogv
MOVIE_URL=https://archive.org/download/CaminandesLlamigos/Caminandes_%20Llamigos-1080p.ogv

src = videoplayer.c

all: videoplayer.z64

videoplayer.z64: N64_ROM_TITLE="Video Player"
videoplayer.z64: $(BUILD_DIR)/videoplayer.dfs

$(BUILD_DIR)/videoplayer.elf: $(src:%.c=$(BUILD_DIR)/%.o)

# If the movie file could not be encoded, build the ROM without it.
$(BUILD_DIR)/videoplayer.dfs: filesystem/movie.m1v filesystem/movie.wav64
	if [ ! -s "$<" ]; then rm -f "$<" ; fi
	$(N64_MKDFS) "$@" filesystem >/dev/null

# Re-encode the movie's video stream if ffmpeg is available
filesystem/movie.m1v: $(MOVIE_FILE)
	@mkdir -p "$(dir $@)"
	if [ -s "$<" ]; then \
		if command -v ffmpeg; then \
			ffmpeg -y -i "$<" -vb 800K -vf 'scale=320x176' -r 20 "$@" ; \
		else \
			echo "videoplayer: Skipping video re-encoding because ffmpeg is not installed" ; \
			touch "$@" ; \
		fi ; \
	else \
		echo "videoplayer: Skipping video re-encoding because the sample movie is missing" ; \
		touch "$@" ; \
	fi

# Convert the movie's audio if it was extracted to wav
filesystem/movie.wav64: movie.wav
	@mkdir -p "$(dir $@)"
	if [ -s "$<" ]; then \
		$(N64_AUDIOCONV) -o "$(dir $@)" "$<" ; \
	else \
		echo "videoplayer: Skipping audio conversion because the audio was not extracted" ; \
		touch "$@" ; \
	fi

# Extract the movie's audio stream to wav if ffmpeg is available
movie.wav: $(MOVIE_FILE)
	if [ -s "$<" ]; then \
		if command -v ffmpeg; then \
			ffmpeg -y -i "$<" -vn -acodec pcm_s16le -ar 32000 -ac 1 "$@" ; \
		else \
			echo "videoplayer: Skipping audio extraction because ffmpeg is not installed" ; \
			touch "$@" ; \
		fi ; \
	else \
		echo "videoplayer: Skipping audio extraction because the sample movie is missing" ; \
		touch "$@" ; \
	fi

# Download the movie file if wget or curl is available
$(MOVIE_FILE):
	if command -v wget ; then \
		wget -O "$@" "$(MOVIE_URL)"  ; \
	elif command -v curl ; then \
		curl -Lo "$@" "$(MOVIE_URL)" ; \
	else \
		echo "videoplayer: Skipping sample movie download because wget or curl are not installed" ; \
		touch "$@" ; \
	fi

clean:
	rm -rf $(BUILD_DIR) $(MOVIE_FILE) *.wav filesystem videoplayer.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
