INSTALLDIR = $(N64_INST)
CFLAGS += -std=gnu99 -O2 -Wall -Werror -Wno-unused-result -I../../include
LDFLAGS += -lm
all: mkextern

mkextern: mkextern.c
	$(CC) $(CFLAGS)  mkextern.c -o mkextern $(LDFLAGS)

install: mkextern
	install -m 0755 mkextern $(INSTALLDIR)/bin

.PHONY: clean install

clean:
	rm -rf mkextern
