# Libdragon

[![Build](https://github.com/DragonMinded/libdragon/actions/workflows/ci.yml/badge.svg?branch=trunk)](https://github.com/DragonMinded/libdragon/actions/workflows/ci.yml)

## Welcome to libdragon

Libdragon is an open-source SDK for Nintendo 64. It aims for a complete N64
programming experience while providing programmers with modern approach to
programming and debugging. These are the main features:

* Based on modern GCC (12.1) and Newlib, for a full C11 programming experience.
  A Docker container is available to quickly set up the programming environment.
* The GCC toolchain is 64 bit capable to be able to use the full R4300 capabilities
  (commercial games and libultra are based on a 32-bit ABI and is not possible
  to use 64-bit registers and opcodes with it)
* Can be developed with newer-generation emulators (cen64, Ares, Dillonb's n64,
  m64p) or development cartridges (64drive, EverDrive64).
* Support both vanilla N64 and iQue Player (chinese variant). The support is
  experimental and done fully at runtime, so it is possible to run ROMs built
  with libdragon on iQue without modifying the source code.
* In-ROM filesystem implementation for assets. Assets can be loaded with
  `fopen("rom://asset.dat")` without having to do complex things to link them in.
* Efficient interrupt-based timer library (also features a monotone 64-bit
  timer to avoid dealing with 32-bit overflows)
* Graphics: easy-to-use API for 2D games, accelerated with RDP
* Support for standard N64 controllers and memory paks.
* Support for saving to flashes and EEPROMs (including a mini EEPROM
  filesystem to simplify serialization of structures).
* Audio: advanced RSP-accelerated library, supporting up to 32 channels and
  streaming samples from ROM during playback for very low memory usage.
  Supports WAV files for sound effects and the XM (FastTracker, MilkyTracker,
  OpenMPT), and YM (Arkos Tracker 2) module formats for background music. 
  Can playback a 10-channel XM with < 3% CPU and < 10% RSP.
* Debugging aids: console (printf goes to screen) exception screen, many
  asserts (so that you get a nice error screen instead of a console lockup),
  `fprintf(stderr)` calls are redirected to your PC console in emulators
  and to USB via compatible tools (UNFLoader, g64drive).
* Support to read/write to SD cards in development kits (64drive, EverDrive64),
  simply with `fopen("sd://sdata.dat")`
* Simple and powerful Makefile-based build system for your ROMs and assets
  (n64.mk)

## Getting started: how to build a ROM

### Option 1: Use the libdragon CLI with Docker (Windows, macOS, Linux)

See [the libdragon CLI](https://github.com/anacierdem/libdragon-docker) to
quickly get libdragon up and running. Basically:

1. Download the CLI (as a pre-built binary, or build from source)
2. Run `libdragon init` to create a skeleton project
3. Run `libdragon make` to compile a build a ROM

If you want, you can also compile and run one of the examples that will
be found in `libdragon/examples` in the skeleton project.

### Option 2: Compile the toolchain (Linux only)

1. Create a directory and copy the `build-toolchain.sh` script there from the `tools/` directory.
2. Read the comments in the build script to see what additional packages are needed.
3. Run `./build-toolchain.sh` from the created directory, let it build and install the toolchain.
4. Install libpng-dev if not already installed.

*Below steps can also be executed by running `build.sh` at the top level.*

5. Install libdragon by typing `make install` at the top level.
6. Install the tools by typing `make tools-install` at the top level.
7. Install libmikmod for the examples using it. See `build.sh` at the top level for details.
8. Compile the examples by typing `make examples` at the top level.

You are now ready to run the examples on your N64.

## Getting started: how to run a ROM

### Using emulators

libdragon requires a modern N64 emulator (the first generation of emulators
are basically HLE-only and can only play the old commercial games). Suggested
emulators for homebrew developemnt are: [Ares](https://ares-emulator.github.io),
[cen64](https://github.com/n64dev/cen64), [dgb-n64](https://github.com/Dillonb/n64),
[m64p](https://m64p.github.io).

On all the above emulators, you are also able to see in console anything printed
via `fprintf(stderr)`, see the debug library for more information.

### Using a development cartridge on a real N64

All cartridges that are able to load custom ROMs should be able to successfully
load libdragon ROMs via either USB/serial, or from a MMC/SD card. For instance,
the followig are known to work: 64drive, EverDrive64 (all models), SummerCart 64.

If your cartridge has USB support, use one of the loaders that implement the
libdragon debugging protocol, so to be able to show logs in console. For instance,
[UNFLoader](https://github.com/buu342/N64-UNFLoader), [g64drive](https://github.com/rasky/g64drive),
[ed64](https://github.com/anacierdem/ed64). The official loaders provided by
the vendors are usually less feature-rich.

## Documentation

 * [API reference](https://dragonminded.github.io/libdragon/ref/modules.html)
 * [Examples](https://github.com/DragonMinded/libdragon/tree/trunk/examples)
