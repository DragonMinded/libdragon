INSTALLDIR = $(N64_INST)
CFLAGS = -std=gnu99 -O2 -Wall -I../../include -lpng

all: mksprite convtool

mksprite: mksprite.c
convtool: convtool.c

install: mksprite convtool
	install -D --mode=755 mksprite $(INSTALLDIR)/bin
	install -D --mode=755 convtool $(INSTALLDIR)/bin

.PHONY: clean install

clean:
	rm -rf mksprite
	rm -rf convtool
