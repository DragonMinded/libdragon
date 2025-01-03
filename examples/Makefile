EXAMPLES += audioplayer
EXAMPLES += cpptest
EXAMPLES += ctest 
EXAMPLES += dfsdemo
EXAMPLES += eepromfstest
EXAMPLES += mixertest
EXAMPLES += cpaktest
EXAMPLES += cpak-utest
EXAMPLES += overlays
EXAMPLES += rdpqdemo
EXAMPLES += rspqdemo
EXAMPLES += spritemap
EXAMPLES += test
EXAMPLES += timers
EXAMPLES += vrutest
EXAMPLES += vtest
EXAMPLES += ucodetest

all: $(EXAMPLES)
clean: $(foreach example,$(EXAMPLES),$(example)-clean)
.PHONY: all clean

define EXAMPLE_template
$(1):
ifdef BASE_DIR
	$$(MAKE) -C $(1) SOURCE_DIR=$(BASE_DIR)/$(1)
else
	$$(MAKE) -C $(1)
endif
$(1)-clean:
	$$(MAKE) -C $(1) clean
.PHONY: $(1) $(1)-clean
endef

$(foreach example,$(EXAMPLES),$(eval $(call EXAMPLE_template,$(example))))
