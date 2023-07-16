INSTALLDIR = $(N64_INST)
CFLAGS += -std=gnu99 -O2 -Wall -Werror -Wno-unused-result -I../../include
all: mkfont

mkfont: mkfont.c
	@echo "    [TOOL] mkfont"
	$(CC) $(CFLAGS) mkfont.c -o mkfont -lm

install: mkfont
	install -m 0755 mkfont $(INSTALLDIR)/bin

.PHONY: clean install

clean:
	rm -rf mkfont
