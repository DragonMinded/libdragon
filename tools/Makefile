INSTALLDIR ?= $(N64_INST)

all: chksum64 dumpdfs ed64romconfig mkdfs mksprite n64tool audioconv64

.PHONY: install
install: chksum64 ed64romconfig n64tool audioconv64
	install -m 0755 chksum64 ed64romconfig n64tool $(INSTALLDIR)/bin
	$(MAKE) -C dumpdfs install
	$(MAKE) -C mkdfs install
	$(MAKE) -C mksprite install
	$(MAKE) -C audioconv64 install

.PHONY: clean
clean:
	rm -rf chksum64 ed64romconfig n64tool
	$(MAKE) -C dumpdfs clean
	$(MAKE) -C mkdfs clean
	$(MAKE) -C mksprite clean
	$(MAKE) -C audioconv64 clean

chksum64: chksum64.c
	gcc -o chksum64 chksum64.c

n64tool: n64tool.c
	gcc -o n64tool n64tool.c

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

.PHONY: audioconv64
audioconv64:
	$(MAKE) -C audioconv64
