INSTALLDIR = $(N64_INST)
CFLAGS = -std=gnu99 -O2 -Wall -Wno-unused-result -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -I../../include

all: dumpdfs

dumpdfs: dumpdfs.c

install: dumpdfs
	install -m 0755 dumpdfs $(INSTALLDIR)/bin

.PHONY: clean install

clean:
	rm -rf dumpdfs
