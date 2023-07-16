INSTALLDIR = $(N64_INST)
CFLAGS = -std=gnu99 -O2 -Wall -Werror -I../../include

all: mkdfs

mkdfs: mkdfs.c
	@echo "    [TOOL] mkdfs"
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@ 

install: mkdfs
	install -m 0755 mkdfs $(INSTALLDIR)/bin

.PHONY: clean install

clean:
	rm -rf mkdfs
