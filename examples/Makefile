EXAMPLES += audioplayer
EXAMPLES += brew-volley
#EXAMPLES += compression # Disabled due to missing assets
EXAMPLES += cpptest
EXAMPLES += ctest
EXAMPLES += customfont
#EXAMPLES += dfsdemo # Disabled for now because it requires libmikmod
EXAMPLES += eepromfstest
EXAMPLES += fontdemo
EXAMPLES += gldemo
EXAMPLES += loadspritefromsd
EXAMPLES += mixertest
EXAMPLES += mptest
EXAMPLES += mputest
EXAMPLES += overlays
EXAMPLES += pixelshader
EXAMPLES += rdpqdemo
EXAMPLES += rspqdemo
EXAMPLES += rtctest
EXAMPLES += spritemap
EXAMPLES += test
EXAMPLES += timers
#EXAMPLES += videoplayer # Disabled due to missing assets
EXAMPLES += vrutest
EXAMPLES += vtest

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
