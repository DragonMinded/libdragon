# Libdragon

[![Build](https://github.com/DragonMinded/libdragon/actions/workflows/ci.yml/badge.svg?branch=trunk)](https://github.com/DragonMinded/libdragon/actions/workflows/ci.yml)

## Welcome to libdragon

Libdragon is an open-source SDK for Nintendo 64. It aims for a complete N64
programming experience while providing programmers with modern approach to
programming and debugging. These are the main features:

* Based on modern GCC (12.2) and Newlib, for a full C11 programming experience.
  A Docker container is available to quickly set up the programming environment.
* The GCC toolchain is 64 bit capable to be able to use the full R4300 capabilities
  (commercial games and libultra are based on a 32-bit ABI and is not possible
  to use 64-bit registers and opcodes with it)
* Can be developed with newer-generation emulators (cen64, Ares, Dillonb's n64,
  simple64) or development cartridges (64drive, EverDrive64).
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

### Option 1: Use the libdragon CLI with Docker (Windows, macOS x86/arm, Linux)

See [the libdragon CLI](https://github.com/anacierdem/libdragon-docker) to
quickly get libdragon up and running. Basically:

1. Make sure that you have Docker installed correctly (on Windows and Mac, use
   Docker Desktop). You can run `docker system info` to check that it is working
   correctly.
2. Install the [the libdragon CLI](https://github.com/anacierdem/libdragon-docker).
   You have two options:

   1. Download the [pre-built binary](https://github.com/anacierdem/libdragon-docker/releases/tag/v10.8.0), 
      and copy it into some directory which is part of your system PATH.
   2. If you have `npm` installed (at least verstion 14), run `npm install -g libdragon`.
3. Run `libdragon init` to create a skeleton project
4. Run `libdragon make` to compile a build a ROM

If you want, you can also compile and run one of the examples that will
be found in `libdragon/examples` in the skeleton project.

Note for Apple Silicon users: we ship and update the docker container image for both
x86-64 and arm64, so this option works on Apple Silicon machines too.

### Option 2: Compile the toolchain (Windows WSL2, macOS x86/arm, Linux)

These instructions work for Linux, macOS (Intel / Apple Silicon) and Windows with WSL2.
WSL1 users must [upgrade to WSL2 first](https://docs.microsoft.com/en-us/windows/wsl/install#upgrade-version-from-wsl-1-to-wsl-2).

1. Export the environment variable N64_INST to the path where you want your
   toolchain to be installed. For instance: `export N64_INST=/opt/n64` or
   `export N64_INST=/usr/local`.
2. Enter the `tools` directory. Read the comments at the top of `./build-toolchain.sh` 
   script to see what additional packages are needed. 
   If you are on macOS, make sure [homebrew](https://brew.sh) is installed.
3. Make sure you have at least 7 Gb of disk space available (notice that after
   build, only about 300 Mb will be used, but during build a lot of space is
   required).
4. Run `./build-toolchain.sh` from the `tools` directory, let it build and
   install the toolchain. The process will take a while depending on your computer
   (1 hour is not unexpected).
5. Install libpng-dev if not already installed.
6. Make sure that you still have the `N64_INST` variable pointing to the correct
   directory where the toolchain was installed (`echo $N64_INST`).
7. Run `./build.sh` at the top-level. This will install libdragon, its tools,
   and also build all examples.

You are now ready to run the examples on your N64 or emulator.

Once you are sure everything is fine, you can delete the `tools/toolchain/`
directory, where the toolchain was built. This will free around 6Gb of space.
You will only need the installed binaries in the `N64_INST` from now on.

## Getting started: how to run a ROM

### Using emulators

libdragon requires a modern N64 emulator (the first generation of emulators
are basically HLE-only and can only play the old commercial games). Suggested
emulators for homebrew developemnt are: [Ares](https://ares-emulator.github.io),
[cen64](https://github.com/n64dev/cen64), [dgb-n64](https://github.com/Dillonb/n64),
[simple64](https://simple64.github.io).

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
