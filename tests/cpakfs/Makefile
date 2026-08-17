EXE = $(if $(findstring Windows,$(OS)),.exe,)
CFLAGS += -Wall -Werror -std=gnu11
CFLAGS += -I ../../include
# Host unit test of preview cpakfs path APIs
CFLAGS += -DLIBDRAGON_PREVIEW=2

all: test_cpakfs_path$(EXE)

test_cpakfs_path$(EXE): test_cpakfs_path.o
	@echo "    [CC] $<"
	$(CC) $(LDFLAGS) -o $@ $<


test: test_cpakfs_path$(EXE)
	@echo "    [RUN] $<"
	./$<

ifneq ($(V),1)
.SILENT:
endif
.PHONY: all test
