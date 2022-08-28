INSTALLDIR = $(N64_INST)
CFLAGS = -std=gnu11 -MMD -O2 -Wall -Wno-unused-result -Werror -I../../include
LDFLAGS += -lm

all: rdpvalidate

rdpvalidate: rdpvalidate.c ../../src/rdpq/rdpq_debug.c
	$(CC) $(CFLAGS) rdpvalidate.c ../../src/rdpq/rdpq_debug.c $(LDFLAGS) -o $@ 

install: rdpvalidate
	install -m 0755 rdpvalidate $(INSTALLDIR)/bin

.PHONY: clean install

clean:
	rm -rf rdpvalidate *.o *.d

-include $(wildcard *.d)
