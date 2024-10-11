#!/usr/bin/python
#
# Convert the png font into a packed 5x8 binary array.
# The font is monochrome (we use the alpha channel).
# We pack 8 pixels into 8 bits, vertically; so each
# character takes 5 bytes.
#
from PIL import Image

im = Image.open("font.png", "r")
W = im.width

def pix(x,y):
    return im.getpixel((x,y))[3]

out = open("font.bin", "wb")
for ch in range(36): # 10 numbers + 26 uppercase
    x0 = ch * 8
    for x in range(5):
        b = 0
        for y in range(8):
            if pix(x0+x,y):
                b |= 1 << y
        out.write(bytes([b]))

out.close()
