== Libdragon IPL3 ==

This directory contains the source code of libdragon's IPL3,
an open source bootcode required to be shipped in each ROM to
perform the final phases of hardware initializations
(specifically, RDRAM initialization and configuration) and
actually loading the main binary and run it.

Being the last stage of Nintendo 64's secure boot sequence,
IPL3 must be signed with a cryptographic hash that is currently
still expensive to compute (through GPU bruteforcing). As
a workaround to help development, a signed "trampoline" is
used. See "boot.asm" for more information. The signed
trampoline is committed as "boot_signed.z64" and
used automatically by the Makefile.

=== Building IPL3 ===

To build the standard libdragon IPL3, simply run:

```
    $ make
```

Make sure you have libdragon toolchain installed first,
as required for standard libdragon development. This
will generate two files:

 * ipl3_dev.z64. This is the development version, which
   contains the signed trampoline.
 * ipl3_unsigned.z64. This is production version, without
   trampoline. It will not work as-is because it not signed.

To test the newly built development version of IPL3, there
are two possible ways:

 * Add a line `N64_ROM_HEADER=<path/to/ipl3_dev.z64>` to the
   Makefile of your game using n64.mk. This will tell
   n64.mk to use the custom header instead of the default
   one.
 * Run `make install_dev`. This will modify the source code
   of n64tool to embed this new IPL3 with it by default.
   You then need to rebuild and install n64tool (run
   `make install` in the `tools` directory), and then you
   can finally rebuild your own ROM, that will use the
   updated file.
