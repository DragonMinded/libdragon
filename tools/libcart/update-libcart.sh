#!/bin/bash
set -euo pipefail
rm -rf libcart/
git clone --quiet https://github.com/devwizard64/libcart
python cat.py ../../src/libcart/cart.c \
    libcart/src/cartint.h \
    libcart/src/cart.c \
    libcart/src/cartbuf.c \
    libcart/src/sd.h \
    libcart/src/sd.c \
    libcart/src/sdcrc16.c \
    libcart/src/cartinit.c \
    libcart/src/cartexit.c \
    libcart/src/cartcardinit.c \
    libcart/src/cartcardrddram.c \
    libcart/src/cartcardrdcart.c \
    libcart/src/cartcardwrdram.c \
    libcart/src/cartcardwrcart.c \
    libcart/src/ci.h \
    libcart/src/ci.c \
    libcart/src/ciinit.c \
    libcart/src/ciexit.c \
    libcart/src/cicardinit.c \
    libcart/src/cicardrddram.c \
    libcart/src/cicardrdcart.c \
    libcart/src/cicardwrdram.c \
    libcart/src/cicardwrcart.c \
    libcart/src/edx.h \
    libcart/src/edxinit.c \
    libcart/src/edxexit.c \
    libcart/src/edxcard.c \
    libcart/src/edxcardinit.c \
    libcart/src/edxcardrddram.c \
    libcart/src/edxcardrdcart.c \
    libcart/src/edxcardwrdram.c \
    libcart/src/edxcardwrcart.c \
    libcart/src/ed.h \
    libcart/src/edinit.c \
    libcart/src/edexit.c \
    libcart/src/edcard.c \
    libcart/src/edcardinit.c \
    libcart/src/edcardrddram.c \
    libcart/src/edcardrdcart.c \
    libcart/src/edcardwrdram.c \
    libcart/src/edcardwrcart.c \
    libcart/src/sc.h \
    libcart/src/sc.c \
    libcart/src/scinit.c \
    libcart/src/scexit.c \
    libcart/src/sccardinit.c \
    libcart/src/sccardrddram.c \
    libcart/src/sccardrdcart.c \
    libcart/src/sccardwrdram.c \
    libcart/src/sccardwrcart.c
python cat.py ../../src/libcart/cart.h libcart/include/*.h
echo Libcart updated. Commit with:
echo git commit --message \"Libcart updated to version $(git -C libcart rev-parse --short HEAD)\" -- ../../src/libcart/*
