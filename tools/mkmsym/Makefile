INSTALLDIR = $(N64_INST)
CFLAGS += -std=gnu99 -g -Wall -Werror -Wno-unused-result -I../../include
LDFLAGS += -lm
all: mkmsym

mkmsym: mkmsym.c
	$(CC) $(CFLAGS)  mkmsym.c -o mkmsym $(LDFLAGS)

install: mkmsym
	install -m 0755 mkmsym $(INSTALLDIR)/bin

.PHONY: clean install

clean:
	rm -rf mkmsym
