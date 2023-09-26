EXAMPLES = audioplayer cpptest ctest dfsdemo eepromfstest mixertest mptest mputest rspqdemo spritemap test timers vrutest vtest ucodetest

all: $(EXAMPLES)
clean: $(foreach example,$(EXAMPLES),$(example)-clean)
.PHONY: all clean

define EXAMPLE_template
$(1):
	$$(MAKE) -C $(1)
$(1)-clean:
	$$(MAKE) -C $(1) clean
.PHONY: $(1) $(1)-clean
endef

$(foreach example,$(EXAMPLES),$(eval $(call EXAMPLE_template,$(example))))
