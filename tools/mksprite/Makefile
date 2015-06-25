INSTALLDIR = $(N64_INST)
CFLAGS = -std=gnu99 -O2 -Wall -Werror -I../../include -lpng

all: mksprite convtool

mksprite: mksprite.c
convtool: convtool.c

install: mksprite convtool
	install -m 0755 mksprite $(INSTALLDIR)/bin
	install -m 0755 convtool $(INSTALLDIR)/bin

.PHONY: clean install

clean:
	rm -rf mksprite
	rm -rf convtool
