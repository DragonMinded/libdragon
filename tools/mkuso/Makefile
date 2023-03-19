INSTALLDIR = $(N64_INST)
CFLAGS += -std=gnu99 -O2 -Wall -Werror -Wno-unused-result -I../../include
LDFLAGS += -lm
all: mkuso

mkuso: mkuso.c ../common/assetcomp.h ../common/assetcomp.c
	$(CC) $(CFLAGS)  mkuso.c -o mkuso $(LDFLAGS)

install: mkuso
	install -m 0755 mkuso $(INSTALLDIR)/bin

.PHONY: clean install

clean:
	rm -rf mkuso
