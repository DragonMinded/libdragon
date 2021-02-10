INSTALLDIR = $(N64_INST)

all: build
build: chksum64 n64tool dumpdfs mkdfs mksprite
install: dumpdfs-install mkdfs-install mksprite-install chksum64-install n64tool-install
clean: chksum64-clean n64tool-clean dumpdfs-clean mkdfs-clean mksprite-clean

chksum64: chksum64.c
	gcc -o chksum64 chksum64.c
chksum64-install: chksum64
	install -m 0755 chksum64 $(INSTALLDIR)/bin
chksum64-clean:
	rm -rf chksum64

n64tool: n64tool.c
	gcc -o n64tool n64tool.c
n64tool-install: n64tool
	install -m 0755 n64tool $(INSTALLDIR)/bin
n64tool-clean:
	rm -rf n64tool

dumpdfs:
	+make -C dumpdfs
dumpdfs-install:
	make -C dumpdfs install
dumpdfs-clean:
	make -C dumpdfs clean

mkdfs:
	+make -C mkdfs
mkdfs-install:
	make -C mkdfs install
mkdfs-clean:
	make -C mkdfs clean

mksprite:
	+make -C mksprite
mksprite-install:
	make -C mksprite install
mksprite-clean:
	make -C mksprite clean

.PHONY: dumpdfs mkdfs mksprite
.PHONY: chksum64-install n64tool-install dumpdfs-install mkdfs-install mksprite-install
.PHONY: chksum64-clean n64tool-clean dumpdfs-clean mkdfs-clean mksprite-clean
