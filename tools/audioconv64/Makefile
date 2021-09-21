INSTALLDIR = $(N64_INST)
CFLAGS = -std=gnu11 -MMD -O2 -Wall -Wno-unused-result -Werror -I../../include 

all: audioconv64

audioconv64: audioconv64.c
	$(CC) $(CFLAGS) -o $@ $<

install: audioconv64
	install -m 0755 audioconv64 $(INSTALLDIR)/bin

.PHONY: clean install

clean:
	rm -rf audioconv64 *.o *.d

-include $(wildcard *.d)
