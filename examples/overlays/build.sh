#!/bin/bash

function compile_ovl {
    echo "Building file $1(output $2)"
    $N64_INST/bin/mips64-elf-gcc -c -march=vr4300 -mtune=vr4300 -falign-functions=32 -ffunction-sections -fdata-sections -I$N64_INST/mips64-elf/include -g -O2 -mno-gpopt -std=gnu99 -o $2.o $1
    $N64_INST/bin/mips64-elf-ld -Ur -Tpartial.ld -o $2 $2.o
}

mkdir -p build
if [ -z $N64_INST ]; then
    echo "$N64_INST not set up properly"
    exit 1
fi

compile_ovl circle.c build/circle.plf
compile_ovl n64brew.c build/n64brew.plf
compile_ovl triangle.c build/triangle.plf
echo "Building USO files"
$N64_INST/bin/mkuso -o filesystem build/circle.plf build/n64brew.plf build/triangle.plf
echo "Building symbol files"
$N64_INST/bin/n64sym build/circle.plf filesystem/circle.uso.sym
$N64_INST/bin/n64sym build/n64brew.plf filesystem/n64brew.uso.sym
$N64_INST/bin/n64sym build/triangle.plf filesystem/triangle.uso.sym
make