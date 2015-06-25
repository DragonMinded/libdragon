INSTALLDIR = $(N64_INST)

all: build
build: dumpdfs mkdfs mksprite chksum64 n64tool
clean: chksum64-clean n64tool-clean dumpdfs-clean mkdfs-clean mksprite-clean

chksum64: chksum64.c
	gcc -o chksum64 chksum64.c
chksum64-clean:
	rm -rf chksum64

n64tool: n64tool.c
	gcc -o n64tool n64tool.c
n64tool-clean:
	rm -rf n64tool

dumpdfs:
	make -C dumpdfs
dumpdfs-install:
	make -C dumpdfs install
dumpdfs-clean:
	make -C dumpdfs clean

mkdfs:
	make -C mkdfs
mkdfs-install:
	make -C mkdfs install
mkdfs-clean:
	make -C mkdfs clean

mksprite:
	make -C mksprite
mksprite-install:
	make -C mksprite install
mksprite-clean:
	make -C mksprite clean

install: dumpdfs-install mkdfs-install mksprite-install
	install -m 0755 chksum64 $(INSTALLDIR)/bin
	install -m 0755 n64tool $(INSTALLDIR)/bin

.PHONY: dumpdfs mkdfs mksprite dumpdfs-install mkdfs-install mksprite-install chksum64-clean n64tool-clean 
.PHONY: dumpdfs-clean mkdfs-clean mksprite-clean
