# Libdragon

<p align="center">
<img src="https://user-images.githubusercontent.com/127010686/256988697-8fe1bdcf-a21b-42d0-b383-502eaf048b50.png#gh-dark-mode-only" width="400">
<img src="https://user-images.githubusercontent.com/127010686/256988620-1167a1e7-6773-4a67-97d4-d251c12ef8ba.png#gh-light-mode-only" width="400">
</p>

[![Build](https://github.com/DragonMinded/libdragon/actions/workflows/build-toolchain-library-and-roms.yml/badge.svg?branch=trunk)](https://github.com/DragonMinded/libdragon/actions/workflows/build-toolchain-library-and-roms.yml)

## Welcome to libdragon

> [!TIP]
> Coming back here after a while? Check the [ChangeLog](https://github.com/DragonMinded/libdragon/wiki/Stable-branch--Changelog) of our stable branch, or the [Preview branch](https://github.com/DragonMinded/libdragon/wiki/Preview-branch)

Libdragon is an open-source SDK for Nintendo 64. It aims for a complete N64
programming experience while providing programmers with modern approach to
programming and debugging. These are the main features:

* Based on modern GCC (version 14) and Newlib, for a full C11 programming experience.
  A Docker container is available to quickly set up the programming environment.
* The GCC toolchain is 64 bit capable to be able to use the full R4300 capabilities
  (commercial games and libultra are based on a 32-bit ABI and is not possible
  to use 64-bit registers and opcodes with it)
* Can be developed with newer-generation emulators (Ares) and development cartridges
  (64drive, EverDrive64, SummerCart64).
* Support both vanilla N64 and iQue Player (Chinese variant). It is possible
  to run ROMs built with libdragon on iQue without modifying the source code.
* 2D accelerated graphics:
   * Comprehensive RDP library called [rdpq](https://github.com/DragonMinded/libdragon/wiki/Rdpq)
     that offers both low-level access and very high-level blitting functions.
   * Support for drawing sprites of arbitrary sizes and arbitrary pixel formats.
     Rdpq takes care of handling TMEM limits transparently and efficiently.
   * Support for sprite zooming and rotation. Rotated sprites are transparently
     drawn via triangles instead of rectangles.
   * Support for all RDP pixel formats, including palettized ones.
   * Very simple render mode configuration, that allows for full RDP graphic effects
     including custom color combiner and blender.
   * Comprehensive [mksprite](https://github.com/DragonMinded/libdragon/wiki/Mksprite)
     tool, that converts from PNG format, includes optional state-of-the-art color
     quantizer and dithering.
   * Transparent compression of graphics for minimal ROM size
* Audio:
   * Advanced RSP-accelerated mixer library, supporting up to 32 channels and
     streaming samples from ROM during playback for very low memory usage.
   * Supports WAV files for sound effects
   * Supports streaming of uncompressed or VADPCM-compressed WAV files for music.
   * Supports playing of XM modules (FastTracker, MilkyTracker, OpenMPT). Can
     playback a 10-channel XM with < 3% CPU and < 10% RSP.
   * Supports playing of YM modules (Arkos Tracker 2)  
* Filesystems:
  * In-ROM filesystem implementation for assets. Assets can be loaded with
    `fopen("rom://asset.dat")` without having to do complex things to link them in.
  * SD card access (`fopen("sd://asset.dat")`) on all available flashcarts.
* [Compression](https://github.com/DragonMinded/libdragon/wiki/Compression):
   * Asset library for fast, transparent compression support for data files,
     including your custom ones.
   * Automatically integrated in conversion tools for graphics.
   * Three different compression algorithms with increasing compression ratio
     (and decreasing decompression speed). Currently based on LZ4, Aplib, Shrinkler.
     Compression ratios competitive with gzip and xz, at higher decompression speeds.
   * Optimized decompression routines in MIPS assembly that run in parallel
     with DMA for maximum speed.
   * Support for streaming decompression based on the `fopen()` interface.
* Debugging: 
   * Clear error screens with symbolized stack traces in case of crashes
   * Codebase is filled with assertions, so that you get a nice error screen
     instead of a console lockup.
   * Printf-debugging via `debugf()` which are redirected to your PC console
     in emulators and to USB via compatible tools (UNFLoader, g64drive).
* Support for standard N64 controllers and memory paks.
* Support for saving to flashes and EEPROMs (including a mini EEPROM
  filesystem to simplify serialization of structures).

The [preview branch](https://github.com/DragonMinded/libdragon/wiki/Preview-branch) features
many more features:

 * 3D graphics
   * Allow for easily plugging in 3D graphics pipelines, that can
     potentially even coexist in the same scene.
   * Included in libdragon: full [OpenGL 1.1 port](https://github.com/DragonMinded/libdragon/wiki/OpenGL-on-N64), together with custom
     N64 extensions for using RDP-specific features.
   * Third-party: [Tiny3D](https://github.com/HailToDodongo/tiny3d), a high-performance native
     3D pipeline.
   * Both OpenGL and Tiny3D import model files from Blender via the GLTF format,
     and feature also an animation system with skinning support.
 * a [MPEG1 RSP-accelerated movie player](https://github.com/DragonMinded/libdragon/wiki/MPEG1-Player), for high-quality FMVs.
   * Expected performance for FMV: 320x240 movie at 800 Kbit/s at 20 fps
   * Very simple to use also for render-to-texture scenarios, where
     a movie is played back as part of a 3D scene or as background in
     a 2D game.
 * Improved boot using open-source IPL3 bootcode, which boots ROMs up to 5x
   faster and allows for compressed game code (using libdragon compression library).
 * Dynamic library support (DSO, sometimes called "overlays") for dynamically
   loading and unloading part of game code and data. This is implemented using
   the standard `dlopen()` / `dlsym()`.
 * Support for [Opus audio compression](https://github.com/DragonMinded/libdragon/wiki/Opus-decompression) for ultra-compressed music streaming.
   * Same state-of-the-art audio algorithm that is currently mainstream on PCs.
   * Compression ratios around 15:1 for audio files. Around 3-5 minutes of mono
     audio per Megabyte of ROM, depending on quality.
   * RSP-accelerated for realtime playback. 18-20% of CPU usage for mono streams,
     which makes it feasible for menus or not resource-intensive games.
   * Well suited also for speech at very high compression ratio, would allow for
     a fully talkie game.

and much more. These features will eventually land to trunk, but you can start playing
with them even today. Go the [preview branch doc](https://github.com/DragonMinded/libdragon/wiki/Preview-branch) for more information.

## Getting started: how to build a ROM

To get started with libdragon, you need to [download and install the toolchain](https://github.com/DragonMinded/libdragon/releases/tag/toolchain-continuous-prerelease).

Make sure to read the [full installation instructions](https://github.com/DragonMinded/libdragon/wiki/Installing-libdragon) which also explain the system requirements.

## Getting started: how to run a ROM

### Using emulators

libdragon targets real N64 hardware and uses many advanced corners
of the hardware not used by old commercial games, and thus requires
a modern N64 emulator which focuses on full hardware emulation.

At the moment, the only emulator that accurately emulates the hardware
(and does not just focus on playing old classics) is [Ares](https://github.com/ares-emulator/ares). Ares requires a modern PC with a discrete
GPU with Vulkan support.

You can develop 99% of your game using libdragon and the Ares emulator,
and be confident that the game will correctly run on hardware as well.
Make sure to turn on the "Homebrew mode" in Ares to enable developer
specific checks during emulation that will simplify the debugging experience.

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
