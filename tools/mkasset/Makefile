INSTALLDIR = $(N64_INST)
CFLAGS += -std=gnu99 -O2 -Wall -Werror -I../../include -MMD

all: mkasset

mkasset: mkasset.c ../common/assetcomp.c
	$(CC) $(CFLAGS) -o $@ mkasset.c ../common/assetcomp.c

install: mkasset
	install -m 0755 mkasset $(INSTALLDIR)/bin

.PHONY: clean install

-include $(wildcard *.d)

clean:
	rm -rf mkasset
