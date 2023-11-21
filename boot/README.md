## Libdragon IPL3

This directory contains the source code of libdragon's IPL3,
an open source bootcode required to be shipped in each ROM to
perform the final phases of hardware initializations
(specifically, RDRAM initialization and configuration) and
actually loading the main binary and run it.

Being the last stage of Nintendo 64's secure boot sequence,
IPL3 must be signed with a cryptographic hash that is currently
still expensive to compute (through GPU bruteforcing). As
a workaround to help development, a pre-signed "trampoline" is
used (see "boot.asm" for more information).

### IPL3 features

 * Searches and loads an ELF file appended to it. All loadable
   ELF segments are loaded in RDRAM and then the entrypoint is called.
 * Very fast. Boots a 350 KiB ELF file in ~165ms (cold boot) or ~71ms (warm boot).
   This is respectively 3 times and 4 times faster than Nintendo's IPL3.
 * ROMs can now be of any size (even smaller than 1 MiB), and no header checksum
   is required.
 * Entropy collection. During the boot process, some entropy is collected
   and made available to the running application in the form of a "true random"
   32-bit integer. This can be used to seed a pseudo random number generator.
 * Support for iQue. With a trampoline trick, this IPL3 also works on the iQue
   console which normally skips IPL3.
 * Written in C code, with comments. Even though the code is extremely
   low-level, care has been taken to make sure it is easily hackable
   (no hidden assumptions, no dependency on weird corner cases of GCC codegen).

### Using IPL3

Libdragon uses this IPL3 by default: it is embedded in the n64tool
application used to build ROMs. Normally, this is done by n64.mk.

To use this IPL3 with your own programming environment, please follow
the following rules:

 * Provide your binary in ELF format, statically-linked. IPL3 reads
   all program headers of type PT_LOAD, and ignores all sections.
 * The ELF file must be concatenated to the IPL3 z64 file (either
   development or production build), respecting a 256-byte alignment.
   You can put any other data before and/or after it, as long as the ELF
   file is 256-byte aligned. IPL3 will linearly scan the ROM searching
   for the ELF file, so the closest the file is to the top of the
   ROM, the fastest it will boot.
 * IPL3 does not read or care about the 64-byte header of the ROM. Feel
   free to change it at your please.
 * IPL3 passes some boot flags to the application in DMEM. This is different
   from Nintendo IPL3 which uses special memory locations at the beginning
   of RDRAM. This is the layout of boot flags, stored at the beginning of DMEM:
   - Byte 0..3: Amount of memory, in bytes. On iQue, this number is adjusted
     to report the amount of memory allocated to the application by the OS.
   - Byte 4..7: 32-bit random number (entropy collected during boot)
   - Byte 8: reserved
   - Byte 9: TV type (0:PAL, 1:NTSC, 2:MPAL)
   - Byte 10: Reset type (0:cold, 1:warm)
   - Byte 11: Console type (0:n64, 1:iQue)
   - Byte 12..15: reserved


### Building IPL3

To build the standard libdragon IPL3, simply run:

```
    $ make
```

Make sure you have libdragon toolchain installed first,
as required for standard libdragon development. 

This will create a file called `ipl3_dev.z64` that contains
the development version of IPL3. Thanks to a special signed
trampoline, this version can immediately be tested with
emulators and on real hardware, and contains full debug
support (logging).

To test the newly built development version of IPL3, there
are two possible ways:

 * Add a line `N64_ROM_HEADER=<path/to/ipl3_dev.z64>` to the
   Makefile of your game using n64.mk. This will tell
   n64.mk to use the custom header instead of the default
   one.
 * Run `make install`. This will modify the source code
   of n64tool to embed this new IPL3 with it by default.
   You then need to rebuild and install `n64tool` (run
   `make install` in the `tools` directory), and then you
   can finally rebuild your own ROM, that will use the
   updated file.


### Building IPL3 (production version)

Building the production version of IPL3 can be done by running
the same Makefile with a different option:

```
    $ make clean      # make sure to rebuild from scratch
    $ make PROD=1
```

This will create a file called `ipl3_prod.z64`. This contains the
non-debug version of IPL3. This version must be correctly signed
before being usable on real hardware and on accurate emulators.
To sign the ROM, the best option is to perform GPU cracking using
[ipl3hasher](https://github.com/awygle/ipl3hasher).

After you have correctly signed the ROM, you can follow the same
instructions above to use it: you can either set `N64_ROM_HEADER`,
or run `make install PROD=1` to embed it into `n64tool`.

### Debugging

The development version of IPL3 is able to produce logs during
its execution, using a barebone logging system. Logging is available
through the following channels:

 * USB log via 64drive, following the UNFLoader protocol
 * USB log via SummerCart64, following the UNFLoader protocol
 * ISViewer, as implemented by emulators like Ares
 * On iQue, logs are written in the save area. Make sure to allocate
   a save type to the ROM (eg: SRAM), and then dump its contents.
