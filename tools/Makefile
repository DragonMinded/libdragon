INSTALLDIR ?= $(N64_INST)

all: chksum64 dumpdfs ed64romconfig mkdfs mksprite mkfont mkasset n64tool n64sym audioconv64 rdpvalidate

.PHONY: install
install: all
	install -m 0755 chksum64 ed64romconfig n64tool n64sym $(INSTALLDIR)/bin
	$(MAKE) -C dumpdfs install
	$(MAKE) -C mkdfs install
	$(MAKE) -C mksprite install
	$(MAKE) -C mkfont install
	$(MAKE) -C mkasset install
	$(MAKE) -C audioconv64 install
	$(MAKE) -C rdpvalidate install

.PHONY: clean
clean:
	rm -rf chksum64 ed64romconfig n64tool n64sym
	$(MAKE) -C dumpdfs clean
	$(MAKE) -C mkdfs clean
	$(MAKE) -C mksprite clean
	$(MAKE) -C mkfont clean
	$(MAKE) -C mkasset clean
	$(MAKE) -C audioconv64 clean
	$(MAKE) -C rdpvalidate clean

chksum64: chksum64.c
	gcc -o chksum64 chksum64.c

n64tool: n64tool.c
	gcc -o n64tool n64tool.c

n64sym: n64sym.c
	gcc -std=gnu99 -O2 -Wall -o n64sym n64sym.c

ed64romconfig: ed64romconfig.c
	gcc -o ed64romconfig ed64romconfig.c

.PHONY: dumpdfs
dumpdfs:
	$(MAKE) -C dumpdfs

.PHONY: mkdfs
mkdfs:
	$(MAKE) -C mkdfs

.PHONY: mksprite
mksprite:
	$(MAKE) -C mksprite

.PHONY: mkfont
mkfont:
	$(MAKE) -C mkfont

.PHONY: mkasset
mkasset:
	$(MAKE) -C mkasset

.PHONY: audioconv64
audioconv64:
	$(MAKE) -C audioconv64

.PHONY: rdpvalidate
rdpvalidate:
	$(MAKE) -C rdpvalidate
