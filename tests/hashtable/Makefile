EXE = $(if $(findstring Windows,$(OS)),.exe,)
CFLAGS += -Wall -Werror -std=gnu11
CFLAGS += -MMD -MP

all: hashtest$(EXE)

hashtest$(EXE): hashtest.c ../../src/hashtable.c
	@echo "    [CC] $<"
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

test: hashtest$(EXE)
	@echo "    [RUN] $<"
	./$<

ifneq ($(V),1)
.SILENT:
endif
.PHONY: all test

-include hashtest.d

