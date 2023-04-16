INSTALLDIR = $(N64_INST)
CFLAGS += -std=gnu99 -O2 -Wall -Werror -Wno-unused-result -Wno-unknown-pragmas -I../../include
LDFLAGS += -lm
all: n64uso n64uso-extern n64uso-msym

n64uso: n64uso.c ../common/assetcomp.h ../common/assetcomp.c
	$(CC) $(CFLAGS) n64uso.c -o n64uso $(LDFLAGS)

n64uso-extern: n64uso-extern.c
	$(CC) $(CFLAGS) n64uso-extern.c -o n64uso-extern $(LDFLAGS)
	
n64uso-msym: n64uso-msym.c
	$(CC) $(CFLAGS) n64uso-msym.c -o n64uso-msym $(LDFLAGS)
	
install: n64uso n64uso-extern n64uso-msym
	install -m 0755 n64uso $(INSTALLDIR)/bin
	install -m 0755 n64uso-extern $(INSTALLDIR)/bin
	install -m 0755 n64uso-msym $(INSTALLDIR)/bin
	
.PHONY: clean install

clean:
	rm -rf n64uso n64uso-extern n64uso-msym
