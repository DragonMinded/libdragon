#!/bin/bash
set -euo pipefail
rm -rf libcart/
git clone --quiet https://github.com/devwizard64/libcart
python cat.py ../../src/libcart/cart.c libcart/src/*.c
python cat.py ../../src/libcart/cart.h libcart/include/*.h
echo Libcart updated to version $(git -C libcart rev-parse HEAD) 
