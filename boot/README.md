## Libdragon IPL3

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
