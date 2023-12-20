EXAMPLES += audioplayer
EXAMPLES += brew-volley
#EXAMPLES += compression # Disabled due to missing assets
EXAMPLES += controllertest
EXAMPLES += cpptest
EXAMPLES += ctest
EXAMPLES += customfont
#EXAMPLES += dfsdemo # Disabled for now because it requires libmikmod
EXAMPLES += eepromfstest
EXAMPLES += fontdemo
EXAMPLES += gldemo
EXAMPLES += joypadtest
EXAMPLES += loadspritefromsd
EXAMPLES += micro-ui
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
EXAMPLES += videoplayer
EXAMPLES += vrutest
EXAMPLES += vtest

all: $(EXAMPLES)
clean: $(foreach example,$(EXAMPLES),$(example)-clean)
.PHONY: all clean

define EXAMPLE_template
$(1):
ifdef $(BASE_DIR)
	$$(MAKE) -C $(1) SOURCE_DIR=$(BASE_DIR)/$(1)
else
	$$(MAKE) -C $(1)
endif
$(1)-clean:
	$$(MAKE) -C $(1) clean
.PHONY: $(1) $(1)-clean
endef

$(foreach example,$(EXAMPLES),$(eval $(call EXAMPLE_template,$(example))))
