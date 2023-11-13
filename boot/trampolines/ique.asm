; iQue trampoline code
; --------------------
; On iQue, the OS does not run the IPL3 when a ROM is booted,
; but rather emulates its behavior by loading the first MiB of
; data from offset 0x1000 in ROM to 0x80000400 in RDRAM (or
; wherever the ROM header entrypoint field points to), and
; jumping to it.
;
; Instead, we need to run our IPL3 code on iQue too, as it
; contains our custom logic to load the main binary in ELF format.
; To do so, we store at 0x1000 this trampoling code, which
; basically just loads the IPL3 from the ROM to DMEM and jumps
; to it.

.n64
.create "ique.bin", 0
.headersize 0x80000400-orga()
iQue:
    lui     $t2, 0xB000
    lui     $t1, 0xA400
    addiu   $t3, $t2, 0xFC0
    addiu   $sp, $t1, 0x1FF0
@@loop:
    lw      $ra, 0x1040($t2)
    sw      $ra, 0x0040($t1)
    addiu   $t2, 4
    bne     $t2, $t3, @@loop
    addiu   $t1, 4
    or      $t1, $0, $0
    addiu   $t2, $0, 0x40
    addiu   $t3, $sp, 0xA4000040-0xA4001FF0
    jr      $t3
    addiu   $ra, $sp, 0xA4001550-0xA4001FF0
.fill 0x80000440-.
.close
