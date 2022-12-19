INSTALLDIR = $(N64_INST)
CFLAGS += -std=gnu99 -O2 -Wall -Werror -Wno-unused-result
all: mkfont

mkfont: mkfont.c
	$(CC) $(CFLAGS) mkfont.c -o mkfont -lm

install: mkfont
	install -m 0755 mkfont $(INSTALLDIR)/bin

.PHONY: clean install

clean:
	rm -rf mkfont
