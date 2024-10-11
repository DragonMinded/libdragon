# IPL3 trampoline code
# --------------------
#
# This is technically just a very small IPL3 which loads
# the real IPL3 from ROM from a fixed address (0x10001000).
# Basically, the ROM layout is:
#
#    Offset     Content
#   --------   ---------
#    0x0000     ROM header
#    0x0040     IPL3 trampoline code (must be signed)
#    0x1000     Empty area
#    0x1040     Real IPL3 (unsigned)
#    0x2000     Rest of ROM
#
# This trampoling is used to workaround the need of signing
# IPL3 during development. Being itself signed, it can run
# an unsigned piece of code and run it.
#
# Credits go to hcs64 for the IPL3 trampoline idea, and
# having first released it (https://github.com/hcs64/boot_stub).
#
# This trampoling is an improvement over the above, and
# was written by devwizard (originally for armips).
#
# This trampoline is completely transparent because it does
# restore the value of the CPU registers at the end of IPL2.
# 
# Register values at the end of IPL2:
# t1 = 0, t2 = 0x40, t3 = 0xA4000040, ra = 0xA4001550

    .set noreorder
    .section .text.boot_trampoline

    # IPL3Trampoline: load the real unsigned IPL3 from ROM, overwriting itself.
    # This code is running from 0xA4000040 in DMEM, which is where IPL3s are
    # loaded.
IPL3Trampoline:
    addiu   $t2, $t3, .Lend-.Lstart-4
    # Copy the code between .Lstart and .Lend to 0xA4000000.
1:  lw      $ra, %lo(.Lstart-0xA4000040)($t3)
    sw      $ra, %lo(0xA4000000-0xA4000040)($t3)
    bne     $t3, $t2, 1b
    addiu   $t3, 4
    # Now jump at 0xA4000000. This will run the code at .Lstart.
    lui     $t1, 0xA400
    lui     $t2, 0xB000
    jr      $t1
    addiu   $t3, $t2, 0xFC0
.Lstart:
    # Load the real IPL3 from ROM (at 0xB0001040), overwriting the trampoline
    # at 0xA4000040.
2:  lw      $ra, 0x1040($t2)
    sw      $ra, 0x0040($t1)
    addiu   $t2, 4
    bne     $t2, $t3, 2b
    addiu   $t1, 4
    # Restore initial values of clobbered registers, then jump to the
    # loaded IPL3 at 0xA4000040.
    or      $t1, $0, $0
    addiu   $t2, $0, 0x40
    addiu   $t3, $sp, 0xA4000040-0xA4001FF0
    jr      $t3
    addiu   $ra, $sp, 0xA4001550-0xA4001FF0
.Lend:

    # Zero fill until the end of the IPL3 size in ROM (0xFC0 bytes).
    .fill 0xFC0-(.Lend-IPL3Trampoline)-8, 1, 0

    # Magic value to "sign" this trampoline, forcing a IPL2 checksum match
    # This has been bruteforced via the ipl3hasher GPU cracker.
    # Many thanks to Danukeru and the n64brew community for donating the GPU time.
    .quad 0x00030e413340ba87
