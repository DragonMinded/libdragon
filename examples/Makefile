EXAMPLES = audioplayer cpptest ctest dfsdemo eepromfstest mixertest mptest mputest rspqdemo spritemap test timers vrutest vtest ucodetest

# Populated by evaluating the EXAMPLE_template macro for each example
all:
clean:

define EXAMPLE_template =
$(1):
	$$(MAKE) -C $(1)
$(1)-clean:
	$$(MAKE) -C $(1) clean
all: $(1)
clean: $(1)-clean
.PHONY: $(1) $(1)-clean
endef

$(foreach example,$(EXAMPLES),$(eval $(call EXAMPLE_template,$(example))))
