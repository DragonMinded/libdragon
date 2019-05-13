all: cpptest ctest dfsdemo mptest mputest spritemap test timers vrutest vtest ucodetest
clean: ctest-clean dfsdemo-clean mptest-clean mputest-clean spritemap-clean test-clean timers-clean vrutest-clean vtest-clean ucodetest-clean

cpptest:
	+make -C cpptest
cpptest-clean:
	make -C cpptest clean

ctest:
	+make -C ctest
ctest-clean:
	make -C ctest clean

dfsdemo:
	+make -C dfsdemo
dfsdemo-clean:
	make -C dfsdemo clean

mptest:
	+make -C mptest
mptest-clean:
	make -C mptest clean

mputest:
	+make -C mputest
mputest-clean:
	make -C mputest clean

spritemap:
	+make -C spritemap
spritemap-clean:
	make -C spritemap clean

test:
	+make -C test
test-clean:
	make -C test clean

timers:
	+make -C timers
timers-clean:
	make -C timers clean

vrutest:
	+make -C vrutest
vrutest-clean:
	make -C vrutest clean

vtest:
	+make -C vtest
vtest-clean:
	make -C vtest clean

ucodetest:
	+make -C ucodetest
ucodetest-clean:
	make -C ucodetest clean

.PHONY: cpptest cpptest-clean ctest ctest-clean dfsdemo dfsdemo-clean mptest mptest-clean mputest mputest-clean spritemap spritemap-clean
.PHONY: test test-clean timers timers-clean vrutest vrutest-clean vtest vtest-clean ucodetest ucodetest-clean
