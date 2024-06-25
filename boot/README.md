## Libdragon IPL3

This directory contains the source code of libdragon's IPL3,
an open source bootcode required to be shipped in each ROM to
perform the final phases of hardware initializations
(specifically, RDRAM initialization and configuration) and
actually loading the main binary and run it.

### IPL3 features

 * Searches and loads an ELF file appended to it. All loadable
   ELF segments are loaded in RDRAM and then the entrypoint is called.
 * Support for compressed ELFs. Using some special ELF flags (see below),
   it is possible to compress an ELF file and provide a decompression
   function (stored in the ELF as well), that IPL3 will know how to invoke.
 * Very fast. Boots a 350 KiB ELF file in ~100ms (cold boot) or ~71ms (warm boot),
   thanks also to the compression support that actually speeds up loading
   (it goes at ~125ms if uncompressed). This is almost 5 times
   faster (cold boot) and 4 times faster (warm boot) than Nintendo's IPL3.
 * ROMs can now be of any size (even smaller than 1 MiB), and no header checksum
   is required. Smaller ROMs are faster to upload especially for developers
   using slower flashcarts and SD cards.
 * The RDRAM is cleared before booting, in both cold and warm boot. This helps
   reducing one common source of difference behavior between emulators and
   real hardware, in case the software accesses uninitialized memory.
 * Entropy collection. During the boot process, some entropy is collected
   and made available to the running application in the form of a "true random"
   32-bit integer. This can be used to seed a pseudo random number generator.
 * Development version available for easy and fast development cycle: using
   a pre-signed trampoline, it is not required to sign the binary at every change.
   Moreover, the development version isn't severely size limited and in fact
   also links a minmal debugging library that allows to log both via USB
   in flashcarts and in emulators.
 * Beautiful visual error screens in case the ELF file is not found or is not
   compatible. The error screen is minimal for size constraint, make sure
   to always use the development version and look at the logging available
   via USB and/or ISViewer.
 * Support for iQue. With a trampoline trick, this IPL3 also works on the iQue
   console which normally skips IPL3.
 * Compat build: a special compatibility build is available to make it easier
   to adapt to old build systems producing a flat file (so that no ELF is
   required).
 * Written in C code, with comments. Even though the code is extremely
   low-level, care has been taken to make sure it is easily hackable
   (no hidden assumptions, no dependency on weird corner cases of GCC codegen).

### ChangeLog

(for each version, the md5 of ipl3_prod.z64 is reported)

r7 (7f014ed41cc53bb1a5e44f211f8c3dec)
* Fix RDRAM clearing to fully cover the whole memory (because of a bug,
  some zones were not cleaned at all before).
* Improve boot time by ~10 ms thanks to some optimizations.
* Correctly configure X2 bit during RDRAM init. No visible change, but it
  seems required, when reading the datasheet.

r6 (954cc30e419ba8561e252cd97f8da0a2)
* Fix iQue boot when the binary is in SA2 context.

r5 (1463a76f789aa2087dc0ba4e93d6c25d)
* Initialize $sp to the end of RDRAM. This is a good default for most use cases.
* Add support for 64-bit ELFs
* Clear MI_MASK before booting the ELF
* While IPL3 is running, the stack is now in DMEM rather than in CPU cache;
  this should help emulators while having basically no effect on runtime.

r4 (d78ddd06d2303d3f76d11de6b6326d69)
* Allow to load ELF sections of odd sizes (before, it would error out). This
  is required for n64elfcompress to produce unpadded compressed sections, since
  adding padding might corrupt data without an in-band terminator (like LZ4
  blocks).

r3 (415c937a90ceee6f99d3d8a84edb42d3)
* Add new ipl3_compat build for maximum backward compatibility and easy of
  integration in existing codebases.
* Fix iQue support by correctly clearing the whole RDRAM there as well
* Add support to iQue hardware RNG as entropy source
* Internal changes to allow in the future to change stage2 without resigning

r2 (8eb4192531d365fe3607be973f2f4eb0)
* Fix silly bug that prevented half of RDRAM from being cleared
* Fix bug causing last bytes of decompressed data to be overwritten with zeros
* Fix error screen display on iQue
* Production build now it's bigger than 4 KiB, to avoid too many constraints
  that reduced development flexibility
* ELF file is now searched through up to 64 MiB
* Instruction and Data Cache are fully reset before jumping to the entrypoint
* High 32 KiB of RDRAM are now cleared before jumping to the entrypoint

r1 (d240dee9f218e48edffa16ab24249ecf)
* First release


### Using IPL3

Libdragon uses this IPL3 by default: it is embedded in the `n64tool`
application used to build ROMs. Normally, this is done via `n64.mk`. So there is
nothing specific to do: standard libdragon applications will use this by default.

To use this IPL3 with your own programming environment, please follow
the following rules:

 * Provide your binary in ELF format, statically-linked. IPL3 reads
   all program headers of type PT_LOAD, and ignores all sections. ELF can be
   either 32-bit or 64-bit, but in the latter case, only the lowest 32-bit
   of the addresses will be read.
 * The ELF file must be concatenated to the IPL3 z64 file (either
   development or production build), respecting a 256-byte alignment.
   You can put any other data before and/or after it, as long as the ELF
   file is 256-byte aligned. IPL3 will linearly scan the ROM searching
   for the ELF file, so the closest the file is to the top of the
   ROM, the fastest it will boot.
   * The ELF file can have multiple PT_LOAD segments, all of them will be loaded.
   * The virtual address of loadable segments must be 8-byte aligned.
   * The file offset of loadable segments must be 2-byte aligned.
   * No segment should try to load itself into the last 64 KiB of RDRAM,
     as those are reserved to IPL3 itself.
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
   - Byte 12..13: Offset of the ELF header in ROM, in 256-byte pages. Multiply
     by 256 to obtain the byte offset.
   - Byte 14..15: reserved
 * Register state at boot is undefined, except for the following:
   - $sp points to the end of the available RDRAM, so it can be used as a stack there. 

If something doesn't work, try using the development version of IPL3
(`ipl3_dev.z64`), that will log warnings and errors on both emulators and
flashcarts. This should help you understand the problem.

### Using IPL3 in compatibility mode

IPL3 also supports a "compatibility mode" (`ipl3_compat.z64`) that is meant to be easier to plug into existing homebrew applications with minimal modifications
to the build system. The differences are:

 * Instead of using a ELF, IPL3 will load 1 MiB (by default) of a flat binary
   from address 0x10001000 in ROM to the entrypoint address specified in the
   header at offset 0x8. This is the historical behavior of official IPL3s.
 * Boot flags will be passed in low RDRAM using the following layout:
   * 0x80000300: TV type
   * 0x8000030C: Reset type
   * 0x80000318: memory size
 * As a special feature, IPL3 will look at the word offset 0x10 in the header 
   to see how much to load from ROM. If the word is 0 or the value is too big
   (bigger than the available memory), it will default to 1 MiB.

Just like the standard IPL3 builds, this build will also clear the whole RDRAM,
and no checksum will be verified, so there is no need to calculate one and put
it into the header. Since no checksum is required, also ROMs using this build
can be of any size, even smaller than 1 MiB.

This build does not support debugging / logging, and no error screen will
ever be shown. It will either boot, or crash.

Note that this build is still designed for homebrew productions. It is not
meant to be a replacement for software not designed for it, such as
old commercial games.

### Building IPL3

Standalone, pre-built binaries are available in the `bin` directory. Both
the development version (`ipl3_dev.z64`), the production version
(`ipl3_prod.z64`) and the compatibility version (`ipl3_compat.z64`) are signed
for CIC 6102, which is the most common one.

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


### Building IPL3 (compatibility version)

Building the production version of IPL3 can be done by running
the same Makefile with a different option:

```
    $ make clean      # make sure to rebuild from scratch
    $ make COMPAT=1
```

Just like the production version, this version must be correctly signed 
before being usable on real hardware and accurate emulators.


### Debugging

The development version of IPL3 is able to produce logs during
its execution, using a barebone logging system. Logging is available
through the following channels:

 * USB log via 64drive, following the UNFLoader protocol
 * USB log via SummerCart64, following the UNFLoader protocol
 * ISViewer, as implemented by emulators like Ares
 * On iQue, logs are written in the save area. Make sure to allocate
   a save type to the ROM (eg: SRAM), and then dump its contents.

### ELF compression

IPL3 supports a custom ELF compression format, that can be used to
squeeze ROMs even more. For maximum flexibility and customization, the
compressed ELF must bring its own decompression code; IPL3 will load the
decompression code (stored in a special segment), and then will use it
to decompress the rest of the segments. IPL3 itself does not ship
any compression algorithm.

For libdragon applications, everything is supported via `n64.mk` and the standard
Makefile; by default, ELF files are compressed with libdragon "level 1"
compression (LZ4 at the time of writing), which is normally so fast that even
speeds up loading compared to an uncompressed file. If you want to change
the compression level, you can set `N64_ROM_ELFCOMPRESS` to the compression
level in your `Makefile` (eg: `N64_ROM_ELFCOMPRESS=3`) .

If you want to compress your own ELF, written without libdragon, the easiest
option is to use libdragon's `n64elfcompress` tool (which has no dependencies
with the rest of libdragon). The tool will compress ELF files using one of
libdragon's builtin compression algorithms, as specified on the command line,
and the resulting file will be loadable by IPL3.

NOTE: at the moment of writing, `n64elfcompress` discards all sections in the
ELF file. If your application require sections to be available at runtime,
then you will need to handle compression in some other means (or modify
`n64elfcompress`).

NOTE: at the moment of writing, `n64elfcompress` does not support ELFs with
headers in 64-bit format.

#### Format details

A compressed ELF contains one or more segments which are made of compressed data.
The corresponding program header follows some special interpretation of some fields:

* `p_vaddr`: this must contain the address in RAM where the compressed
  segment will be loaded.
* `p_paddr`: this must contain the address in RAM where the uncompressed
  segment will be loader. Notice that it's up to the compressor tool to
  make sure that `p_vaddr` and `p_paddr` do not conflict.
* `p_filesz`: this must contain the size of the compressed segment.
* `p_memsz`: like in a standard header, this contains the size of the
  (decompressed) segment in RDRAM, plus some optional trailing space that
  will be cleared (normally used for bss).
* `p_flags`: in addition to the usual `PF_READ` and `PF_EXECUTE` flag, the
  compressed segment must also be marked with the flag `PF_N64_COMPRESSED` (0x1000).

Notice that IPL3 reserves the last 64 KiB of RAM to itself for execution. Do not
step on that area with ELF segments.

The decompression function must be stored in a separate segment. The program
header for it must contain one the following values in the `p_type` field:

 * `PT_N64_DECOMP` (0x64e36341): the decompression function will be loaded 
   at the address specified by the `p_vaddr` field. It is up to the compressor
   to make sure this address does not conflict with the segments being
   decompressed. If `p_vaddr` is 0, the decompression function will be
   loaded within the IPL3 reserved region (last 64 Kib of RDRAM); since the
   exact address is not known (and might also depend on the available RAM
   in the system), the decompression function has to be fully relocatable.

The expected API of the decompression function is the following:

```C
// Decompress the data provided in the input buffer into the output buffer.
// input_size contains the size of the input buffer, that might be needed
// to some algorithms to terminate correctly.
//
// decompress() must return the number of decompressed bytes.
//
// NOTE: this function is called while the data in the input buffer
// is being asynchronously loaded via PI DMA. If you need the whole
// buffer to be fully loaded before starting decompression, make sure
// to wait for the end of the current PI DMA transfer.
int decompress(const uint8_t* input, int input_size, uint8_t *output);
```
