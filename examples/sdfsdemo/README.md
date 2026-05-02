# SD Card I/O Example

An example project that demonstrates how to set up saving and reading a file from an SD card with libdragon.

A text (sav.txt) and a binary file (sav.bin) has been provided as to help demostrate how to read these files. Put these files into the root of your SD card.

Requires a Real N64 Game Console. 

Don't run this on emulators, because they don't support SD cards
 
Press A or B to write or read random numbers to the SD card.

Hold Start and press A or B to write or read example text file.

Press Z to take a RGBA5551 screenshot (.raw)

## Requirements

- A real N64, don't run this on emulators like ares because they don't support SD cards
- A flashcart with SD card support
- Libdragon Preview branch
- MIPS64 C compiler
- Make

## How to Build
This tutorial assumes you have your N64 Toolchain set up including GCC for MIPS.
Make sure you are on the preview branch of libdragon.

Clone this repository with `--recurse-submodules` or if you haven't run:

```bash

git submodule update --init
```
---
Initialize libdragon:
```bash
libdragon init
```
Then run make to build this project:

```bash
libdragon make
```

---