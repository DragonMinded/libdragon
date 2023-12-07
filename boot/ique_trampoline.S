# iQue trampoline code
# --------------------
# On iQue, the OS does not run the IPL3 when a ROM is booted,
# but rather emulates its behavior by loading the first MiB of
# data from offset 0x1000 in ROM to 0x80000400 in RDRAM (or
# wherever the ROM header entrypoint field points to), and
# jumping to it.
#
# Instead, we need to run our IPL3 code on iQue too, as it
# contains our custom logic to load the main binary in ELF format.
# To do so, we store at 0x1000 this trampoling code, which
# basically just loads the IPL3 from the ROM to DMEM and jumps
# to it.

    .set noreorder
    .section .text.ique_trampoline

    # IQUETrampoline: sit at 0x1000 in ROM in case someone (like iQue OS)
    # does not run the IPL3 code, but rather simulate the official IPL3 by 
    # loading the first MiB and jumping to it. This is the entrypoint
    # of the first MiB, and it just jumps to the IPL3 code so that it
    # is always run.
IQUETrampoline:
    lui     $t2, 0xB000
    lui     $t1, 0xA400
    addiu   $t3, $t2, 0xFC0
    addiu   $sp, $t1, 0x1FF0
    # Copy the IPL3 from ROM (0xB0000040) to DMEM (0xA4000040)
1:  lw      $ra, 0x0040($t2)
    sw      $ra, 0x0040($t1)
    addiu   $t2, 4
    bne     $t2, $t3, 1b
    addiu   $t1, 4
    # Restore initial values of clobbered registers, then jump to the
    # loaded IPL3 at 0xA4000040.
    or      $t1, $0, $0
    addiu   $t2, $0, 0x40
    addiu   $t3, $sp, 0xA4000040-0xA4001FF0
    jr      $t3
    addiu   $ra, $sp, 0xA4001550-0xA4001FF0

    .fill 0x40-(.-IQUETrampoline), 1, 0
