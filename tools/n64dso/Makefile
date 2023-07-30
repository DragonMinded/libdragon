INSTALLDIR = $(N64_INST)
CFLAGS += -std=gnu99 -O2 -Wall -Werror -Wno-unused-result -Wno-unknown-pragmas -I../../include
LDFLAGS += -lm
all: n64dso n64dso-extern n64dso-msym

n64dso: n64dso.c ../common/assetcomp.h ../common/assetcomp.c
	@echo "    [TOOL] n64dso"
	$(CC) $(CFLAGS) n64dso.c -o n64dso $(LDFLAGS)

n64dso-extern: n64dso-extern.c
	@echo "    [TOOL] n64dso-extern"
	$(CC) $(CFLAGS) n64dso-extern.c -o n64dso-extern $(LDFLAGS)
	
n64dso-msym: n64dso-msym.c
	@echo "    [TOOL] n64dso-msym"
	$(CC) $(CFLAGS) n64dso-msym.c -o n64dso-msym $(LDFLAGS)
	
install: n64dso n64dso-extern n64dso-msym
	install -m 0755 n64dso $(INSTALLDIR)/bin
	install -m 0755 n64dso-extern $(INSTALLDIR)/bin
	install -m 0755 n64dso-msym $(INSTALLDIR)/bin
	
.PHONY: clean install

clean:
	rm -rf n64dso n64dso-extern n64dso-msym
