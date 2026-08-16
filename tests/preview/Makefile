# Do not set LIBDRAGON_PREVIEW here: the checks need N64_CFLAGS to be exactly
# what a stable project gets, and add -DLIBDRAGON_PREVIEW by themselves for the
# cases that are expected to opt in.
include $(N64_INST)/include/n64.mk

# -ffile-prefix-map carries an embedded quoted path, which would not survive
# being passed down and re-split by the shell. It is irrelevant here anyway,
# as nothing is actually code generated.
CHECK_CFLAGS = $(filter-out -ffile-prefix-map=%,$(N64_CFLAGS))

test:
	@echo "    [RUN] preview"
	N64_CC="$(N64_CC)" N64_CFLAGS="$(CHECK_CFLAGS)" $(SOURCE_DIR)/check.sh

ifneq ($(V),1)
.SILENT:
endif
.PHONY: test
