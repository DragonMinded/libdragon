# Libdragon

<p align="center">
<img src="https://user-images.githubusercontent.com/127010686/256988697-8fe1bdcf-a21b-42d0-b383-502eaf048b50.png#gh-dark-mode-only" width="400">
<img src="https://user-images.githubusercontent.com/127010686/256988620-1167a1e7-6773-4a67-97d4-d251c12ef8ba.png#gh-light-mode-only" width="400">
</p>

[![Build](https://github.com/DragonMinded/libdragon/actions/workflows/build-toolchain-library-and-roms.yml/badge.svg?branch=trunk)](https://github.com/DragonMinded/libdragon/actions/workflows/build-toolchain-library-and-roms.yml)

## Welcome to libdragon

Libdragon is an open-source SDK for Nintendo 64. It aims for a complete N64
programming experience while providing programmers with modern approach to
programming and debugging. These are the main features:

* Based on modern GCC (13) and Newlib, for a full C11 programming experience.
  A Docker container is available to quickly set up the programming environment.
* The GCC toolchain is 64 bit capable to be able to use the full R4300 capabilities
  (commercial games and libultra are based on a 32-bit ABI and is not possible
  to use 64-bit registers and opcodes with it)
* Can be developed with newer-generation emulators (ares, cen64, Dillonb's n64,
  simple64) or development cartridges (64drive, EverDrive64, SummerCart64).
* Support both vanilla N64 and iQue Player (Chinese variant). The support is
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
* Support to read/write to SD cards in development kits (64drive, EverDrive64, SummerCart64),
  simply with `fopen("sd://sdata.dat")`
* Simple and powerful Makefile-based build system for your ROMs and assets
  (n64.mk)

The [preview branch](https://github.com/DragonMinded/libdragon/wiki/Preview-branch) features
many more features:

 * a new comprehensive RDP engine
 * a full OpenGL 1.1 port for 3D graphics programming, with a custom, efficient RSP ucode
   with full T&L support.
 * a MPEG1 RSP-accelerated movie player
 * support for showing source-level stack traces in case of crashes or assertions, including
   source file name and line number.

and much more. These features will eventually land to trunk, but you can start playing
with them even today. Go the [preview branch doc](https://github.com/DragonMinded/libdragon/wiki/Preview-branch) for more information.

## Getting started: how to build a ROM

To get started with libdragon, you need to [download and install the toolchain](https://github.com/DragonMinded/libdragon/releases/tag/toolchain-continuous-prerelease).

Make sure to read the [full installation instructions](https://github.com/DragonMinded/libdragon/wiki/Installing-libdragon) which also explain the system requirements.

## Getting started: how to run a ROM

### Using emulators

libdragon requires a modern N64 emulator (the first generation of emulators
are basically HLE-only and can only play the old commercial games). Suggested
emulators for homebrew developemnt are: [ares](https://ares-emu.net),
[cen64](https://github.com/n64dev/cen64), [dgb-n64](https://github.com/Dillonb/n64),
[simple64](https://simple64.github.io).

On all the above emulators, you are also able to see in console anything printed
via `fprintf(stderr)`, see the debug library for more information.

### Using a development cartridge on a real N64

All cartridges that are able to load custom ROMs should be able to successfully
load libdragon ROMs via either USB/serial, or from a MMC/SD card. For instance,
the following are known to work: 64drive, EverDrive64 (all models), SC64.

If your cartridge has USB support, use one of the loaders that implement the
libdragon debugging protocol, so to be able to show logs in console. For instance,
[UNFLoader](https://github.com/buu342/N64-UNFLoader), [g64drive](https://github.com/rasky/g64drive),
[ed64](https://github.com/anacierdem/ed64). The official loaders provided by
the vendors are usually less feature-rich.

## Libdragon stable vs preview

Currently, there are two main libragon versions: 

 * The **stable** version is the one in the `trunk`. Stable means that we strive never
   to break backward compatibility, that is we will never do changes in a way
   that will impede existing applications to successfully compile and work
   against a newer libdragon version. We feel this is important because otherwise
   we would fragment the homebrew ecosystem too much, and we would leave a trail
   of libdragon-based applications that can't be compiled anymore.
 * The **preview** version is the one in the `preview` branch. This is where most
   development happens first. In fact, features are developed, evolved and
   battle-tested here, before the APIs are stabilized and they are finally
   merged on the trunk. Applications that use the preview branch need to be aware
   that the APIs can break at any time (though we try to avoid *gratuitous* breakage).

## Upgrading libdragon

If you are upgrade the stable version, check the [ChangeLog](https://github.com/DragonMinded/libdragon/wiki/Stable-branch--Changelog)
in the wiki to see latest changes that were merged into the stable version of libdragon.
Also check the wiki page for [common hurdles in upgrading libdragon](https://github.com/DragonMinded/libdragon/wiki/Upgrade-troubleshooting).

If you are upgrading the preview version, instead, remember that some breaking
changes are expected. We do not keep track of those though, so you will have
to check the relevant header files yourself to check what is changed.

## Resources

 * [API reference](https://dragonminded.github.io/libdragon/ref/modules.html)
 * [Examples](https://github.com/DragonMinded/libdragon/tree/trunk/examples)
 * [Wiki](https://github.com/DragonMinded/libdragon/wiki) (contains tutorials
   and troubleshooting guides)
 * [Discord n64brew](https://discord.gg/WqFgNWf)
