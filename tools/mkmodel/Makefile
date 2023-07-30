INSTALLDIR = $(N64_INST)
CFLAGS += -std=gnu99 -O2 -Wall -Werror -Wno-unused-result -I../../include
all: mkmodel

mkmodel: mkmodel.c
	@echo "    [TOOL] mkmodel"
	$(CC) $(CFLAGS) mkmodel.c -o mkmodel -lm

install: mkmodel
	install -m 0755 mkmodel $(INSTALLDIR)/bin

.PHONY: clean install

clean:
	rm -rf mkmodel
