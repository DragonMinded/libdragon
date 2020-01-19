# Libdragon

[![Build Status](https://travis-ci.org/DragonMinded/libdragon.svg?branch=master)](https://travis-ci.org/DragonMinded/libdragon)

This is a simple library for N64 that allows one to code using the gcc compiler suite and nothing else. No proprietary library is needed.

See [docker wrapper](https://github.com/anacierdem/libdragon-docker) to quickly get libdragon up and running.

To get started from scratch, follow the following steps:

1. Create a directory and copy the build script there from the tools/ directory.
2. Read the comments in the build script to see what additional packages are needed.
3. Run ./build from the created directory, let it build and install the toolchain.
4. Install libpng-dev if not already installed.

*Below steps can also be executed by running `build.sh` at the top level.*

5. Make libdragon by typing 'make' at the top level.
6. Install libdragon by typing 'make install' at the top level.
7. Make the tools by typing 'make tools' at the top level.
8. Install the tools by typing 'make tools-install' at the top level.
9.  Install libmikmod for the examples using it. See `build.sh` at the top level for details.
10. Compile the examples by typing 'make examples' at the top level.

You are now ready to run the examples on your N64.
For more information, visit http://www.dragonminded.com/n64dev/

# RSP assembly

Libdragon uses assembly macros to program the RSP chip defined in `ucode.S`. These mainly wrap `cop2`, `lwc2` and `swc2` instructions.

The syntax is similar to that of Nintendo's but with a few changes. For example if we take `vabs` instruction;

    vabs vd, vs, vt[e]

it becomes;

    vabs vd, vs, vt, e

and element (`e`) is always required. It is also similar for load/store instructions. As an example, `sbv` instruction;

    sbv vt[element], offset(base)

becomes;

    sbv vt, element, offset, base

Basically all operands are required and separated by commas.

While using these custom instructions, you should use `$v00`-`$v31` when naming vector registers and `$0`-`$31` when naming scalar registers.