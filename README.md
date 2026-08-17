# Libdragon

<p align="center">
<img src="https://github.com/user-attachments/assets/74c67b9b-ddb7-4527-bc61-86d244205c65#gh-dark-mode-only" id="gh-dark-mode-only" width="400">
<img src="https://github.com/user-attachments/assets/02586355-e89e-4aac-a208-5ae465287bd7#gh-light-mode-only" id="gh-light-mode-only" width="400">
</p>

[![Build](https://github.com/DragonMinded/libdragon/actions/workflows/build-toolchain-library-and-roms.yml/badge.svg?branch=trunk)](https://github.com/DragonMinded/libdragon/actions/workflows/build-toolchain-library-and-roms.yml)

## Welcome to libdragon

> [!TIP]
> Coming back here after a while? Check the [ChangeLog](https://github.com/DragonMinded/libdragon/wiki/Stable-branch--Changelog) of our stable branch, or the [Preview branch](https://github.com/DragonMinded/libdragon/wiki/Preview-branch)

Libdragon is an open-source SDK for Nintendo 64. It aims for a complete N64
programming experience while providing programmers with modern approach to
programming and debugging. These are the main features:

* Based on modern GCC (version 16) and Newlib, for a full C11 programming experience.
  A Docker container is available to quickly set up the programming environment.
* The GCC toolchain is 64 bit capable to be able to use the full R4300 capabilities
  (commercial games and libultra are based on a 32-bit ABI and is not possible
  to use 64-bit registers and opcodes with it)
* Can be developed with newer-generation emulators (Ares, Gopher64) and development cartridges
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
   * Supports streaming using the state-of-the-art [Opus codec](https://github.com/DragonMinded/libdragon/wiki/Opus-decompression),
     for incredibly high compression ratio at realtime playback rate.
   * Supports playing of XM modules (FastTracker, MilkyTracker, OpenMPT). Can
     playback a 10-channel XM with < 3% CPU and < 10% RSP.
   * Supports playing of YM modules (Arkos Tracker 2)  
* Filesystems:
  * In-ROM filesystem implementation for assets. Assets can be loaded with
    `fopen("rom://asset.dat")` without having to do complex things to link them in.
  * SD card access (`fopen("sd://asset.dat")`) on all available flashcarts.
* [Data compression](https://github.com/DragonMinded/libdragon/wiki/Compression):
   * Asset library for fast, transparent compression support for data files,
     including your custom ones.
   * Automatically integrated in conversion tools for graphics.
   * Three different compression algorithms with increasing compression ratio
     (and decreasing decompression speed). Currently based on LZ4, Aplib, Shrinkler.
     Compression ratios competitive with gzip and xz, at higher decompression speeds.
   * Optimized decompression routines in MIPS assembly that run in parallel
     with DMA for maximum speed.
   * Support for streaming decompression based on the `fopen()` interface.
* [Dynamic library support](https://github.com/DragonMinded/libdragon/wiki/DSO-(dynamic-libraries)) 
  (DSO, sometimes called "overlays") for dynamically loading and unloading part of
  game code and data. This is implemented using the standard `dlopen()` / `dlsym()`.
* Debugging:
   * Clear error screens with symbolized stack traces in case of crashes
   * Codebase is filled with assertions, so that you get a nice error screen
     instead of a console lockup.
   * Printf-debugging via `debugf()` which are redirected to your PC console
     in emulators and to USB via compatible tools (UNFLoader, g64drive).
* Support for standard N64 controllers and memory paks.
* Support for saving to flashes and EEPROMs (including a mini EEPROM
  filesystem to simplify serialization of structures).
* Improved boot using open-source IPL3 bootcode, which boots ROMs up to 5x
  faster and allows for compressed game code (using libdragon compression library).

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
 * a RDP-accelerated text engine (rdpq_text), with direct conversion from TrueType.
   * Highly optimized atlas creations for low memory impact and high runtime efficiency
   * Support for outlining of fonts to improve contrast
   * Full layout engine (paragraphs, centering, word-wrapping, etc.)
   * Fully Unicode aware
 * Initial support for multithreading via custom real-time kernel
   * Preemptive threads with priority
   * Mutexes, condition variables, semaphores, queues
   * Support for C11 atomic variables and thread-local storage
   * Runtime stack overflow detection
   * NOTE: at this point, most of libdragon is not thread safe yet, so only
     basic things can be performed in threads.

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

At the moment, the only emulators that accurately emulate the hardware
(and does not just focus on playing old classics) are:
* [Ares](https://github.com/ares-emulator/ares)
* [Gopher64](https://github.com/gopher64/gopher64).

Both require a modern PC with a discrete GPU with Vulkan support. Gopher64
is more performant for gaming.

Ares has more development-oriented features so it is suggested during development.
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

There is a single libdragon version, but not every API in it carries the same
promise. Each API is either **stable** or **preview**:

 * A **stable** API will not break backward compatibility. We will never change
   it in a way that impedes existing applications from compiling and working
   against a newer libdragon version. We feel this is important because
   otherwise we would fragment the homebrew ecosystem too much, and we would
   leave a trail of libdragon-based applications that can't be compiled anymore.
 * A **preview** API is still being developed, evolved and battle-tested. It can
   change or be removed at any time, without notice and without a deprecation
   period (though we try to avoid *gratuitous* breakage). This is where most new
   features start their life, before they are stabilized.

Preview APIs are documented as such (look for the "Preview API" note in the
[documentation](https://libdragon.dev/ref/index.html)), and they are locked by
default: using one is a compile time error that names the API you tried to use.
To unlock them, set `LIBDRAGON_PREVIEW` in your Makefile, *before* including
`n64.mk`:

```makefile
LIBDRAGON_PREVIEW = 1          # allow preview APIs, but warn on each use
# LIBDRAGON_PREVIEW = 2        # fully unlock preview APIs (no diagnostics)
include $(N64_INST)/include/n64.mk
```

Accepted values: `0` (default, hard error), `1` (usable with a compiler
warning), `2` (fully unlocked).

The switch is a property of your project, not of your libdragon installation:
the same installed toolchain builds both stable-only and preview-using projects,
and you can flip it at any time.

There is no separate build of libdragon for preview APIs, so enabling the switch
never changes the behaviour or the memory layout of the stable APIs: it only
changes which APIs you are allowed to call.

When a preview API is considered settled, it is promoted to stable and the
marking is simply removed. Nothing needs to change in your project, and you can
drop `LIBDRAGON_PREVIEW` once you no longer use any preview API.

## Upgrading libdragon

Check the [ChangeLog](https://github.com/DragonMinded/libdragon/wiki/Stable-branch--Changelog)
in the wiki to see the latest changes to the stable APIs.
Also check the wiki page for [common hurdles in upgrading libdragon](https://github.com/DragonMinded/libdragon/wiki/Upgrade-troubleshooting).

If your project enables `LIBDRAGON_PREVIEW`, instead, remember that some
breaking changes are expected. We do not keep track of those though, so you will
have to check the relevant header files yourself to see what has changed.

## Resources

 * [API reference](https://dragonminded.github.io/libdragon/ref/topics.html)
 * [Examples](https://github.com/DragonMinded/libdragon/tree/trunk/examples)
 * [Wiki](https://github.com/DragonMinded/libdragon/wiki) (contains tutorials
   and troubleshooting guides)
 * [Discord n64brew](https://discord.gg/WqFgNWf)
