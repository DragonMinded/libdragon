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
    libcart/src/cart/cartinit.c \
    libcart/src/cart/cartexit.c \
    libcart/src/cart/cartcardinit.c \
    libcart/src/cart/cartcardrddram.c \
    libcart/src/cart/cartcardrdcart.c \
    libcart/src/cart/cartcardwrdram.c \
    libcart/src/cart/cartcardwrcart.c \
    libcart/src/ci/ci.h \
    libcart/src/ci/ci.c \
    libcart/src/ci/ciinit.c \
    libcart/src/ci/ciexit.c \
    libcart/src/ci/cicardinit.c \
    libcart/src/ci/cicardrddram.c \
    libcart/src/ci/cicardrdcart.c \
    libcart/src/ci/cicardwrdram.c \
    libcart/src/ci/cicardwrcart.c \
    libcart/src/edx/edx.h \
    libcart/src/edx/edxinit.c \
    libcart/src/edx/edxexit.c \
    libcart/src/edx/edxcard.c \
    libcart/src/edx/edxcardinit.c \
    libcart/src/edx/edxcardrddram.c \
    libcart/src/edx/edxcardrdcart.c \
    libcart/src/edx/edxcardwrdram.c \
    libcart/src/edx/edxcardwrcart.c \
    libcart/src/ed/ed.h \
    libcart/src/ed/edinit.c \
    libcart/src/ed/edexit.c \
    libcart/src/ed/edcard.c \
    libcart/src/ed/edcardinit.c \
    libcart/src/ed/edcardrddram.c \
    libcart/src/ed/edcardrdcart.c \
    libcart/src/ed/edcardwrdram.c \
    libcart/src/ed/edcardwrcart.c \
    libcart/src/sc/sc.h \
    libcart/src/sc/sc.c \
    libcart/src/sc/scinit.c \
    libcart/src/sc/scexit.c \
    libcart/src/sc/sccardinit.c \
    libcart/src/sc/sccardrddram.c \
    libcart/src/sc/sccardrdcart.c \
    libcart/src/sc/sccardwrdram.c \
    libcart/src/sc/sccardwrcart.c
python cat.py ../../src/libcart/cart.h libcart/include/*.h
echo Libcart updated. Commit with:
echo git commit --message \"Libcart updated to version $(git -C libcart rev-parse --short HEAD)\" -- ../../src/libcart/*
