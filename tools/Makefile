INSTALLDIR ?= $(N64_INST)

all: chksum64 dumpdfs ed64romconfig mkdfs mksprite n64tool

.PHONY: install
install: chksum64 ed64romconfig n64tool
	install -m 0755 chksum64 ed64romconfig n64tool $(INSTALLDIR)/bin
	make -C dumpdfs install
	make -C mkdfs install
	make -C mksprite install

.PHONY: clean
clean:
	rm -rf chksum64 ed64romconfig n64tool
	make -C dumpdfs clean
	make -C mkdfs clean
	make -C mksprite clean

chksum64: chksum64.c
	gcc -o chksum64 chksum64.c

n64tool: n64tool.c
	gcc -o n64tool n64tool.c

ed64romconfig: ed64romconfig.c
	gcc -o ed64romconfig ed64romconfig.c

.PHONY: dumpdfs
dumpdfs:
	+make -C dumpdfs

.PHONY: mkdfs
mkdfs:
	+make -C mkdfs

.PHONY: mksprite
mksprite:
	+make -C mksprite
