INSTALLDIR = $(N64_INST)
CFLAGS = -std=gnu99 -O2 -Wall -Werror -Wno-unused-result -I../../include
LDFLAGS = -lpng
all: mksprite convtool

mksprite:
	$(CC) $(CFLAGS)  mksprite.c -o mksprite $(LDFLAGS)
convtool:
	$(CC) $(CFLAGS)  convtool.c -o convtool $(LDFLAGS)

install: mksprite convtool
	install -m 0755 mksprite $(INSTALLDIR)/bin
	install -m 0755 convtool $(INSTALLDIR)/bin

.PHONY: clean install

clean:
	rm -rf mksprite
	rm -rf convtool
