
all: font.h

font.h: font.bin
	xxd -i font.bin >font.h

font.bin: font.png
	python font_conv.py

.PHONY: all
