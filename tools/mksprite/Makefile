INSTALLDIR = $(N64_INST)
CFLAGS += -std=gnu99 -O2 -Wall -Werror -Wno-unused-result -I../../include
LDFLAGS += -lm
all: mksprite convtool

mksprite:
	@echo "    [TOOL] mksprite"
	$(CC) $(CFLAGS) -MMD mksprite.c -o mksprite $(LDFLAGS)
convtool: convtool.c
	@echo "    [TOOL] convtool"
	$(CC) $(CFLAGS) -MMD convtool.c -o convtool $(LDFLAGS)

install: mksprite convtool
	install -m 0755 mksprite $(INSTALLDIR)/bin
	install -m 0755 convtool $(INSTALLDIR)/bin

.PHONY: clean install

clean:
	rm -rf mksprite
	rm -rf convtool

include $(wildcard *.d)
