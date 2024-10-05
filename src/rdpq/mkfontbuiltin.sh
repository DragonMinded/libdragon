#!/bin/bash
set -euo pipefail

TMPDIR=/tmp
FONTDB=../../examples/fontgallery/assets

# Builtin fonts are monogram and at01, available in the fontgallery example
BUILTIN="monogram at01"

rm -f rdpq_font_builtin.c
for font in $BUILTIN; do
    "$N64_INST/bin/mkfont" --compress 0 --output $TMPDIR --range 20-BF --outline 1  "$@" "$FONTDB/$font.ttf"
    OUTFN="$TMPDIR/$font.font64"
    OUTFN_C=$(echo $OUTFN | sed 's/[/.]/_/g')
    xxd -i $OUTFN | sed "s/${OUTFN_C}/__fontdb_${font}/g" >>rdpq_font_builtin.c
done
