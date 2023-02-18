INSTALLDIR = $(N64_INST)
CFLAGS += -std=gnu99 -O2 -Wall -Werror -Wno-unused-result -I../../include
LDFLAGS += -lpng
all: mksprite convtool

mksprite: mksprite.c lodepng.c lodepng.h ../common/assetcomp.c
	$(CC) $(CFLAGS)  mksprite.c ../common/assetcomp.c -o mksprite
convtool: convtool.c
	$(CC) $(CFLAGS)  convtool.c -o convtool

install: mksprite convtool
	install -m 0755 mksprite $(INSTALLDIR)/bin
	install -m 0755 convtool $(INSTALLDIR)/bin

.PHONY: clean install

clean:
	rm -rf mksprite
	rm -rf convtool
