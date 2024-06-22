#!/bin/bash
set -euo pipefail

TMPDIR=/tmp
FONTDB=../../examples/fontgallery/assets

# Builtin fonts are monogram and at01, available in the fontgallery example
BUILTIN="monogram at01"

rm -f rdpq_font_builtin.c
for font in $BUILTIN; do
    $N64_INST/bin/mkfont -v --compress 0 --output $TMPDIR --range 20-BF --outline 1 "$FONTDB/$font.ttf"
    xxd -i -n __fontdb_$font $TMPDIR/$font.font64 >>rdpq_font_builtin.c
done
