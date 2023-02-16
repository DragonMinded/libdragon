# t1, t2, t3 and ra are constant values from IPL2 that i use here
# t1 = 0, t2 = 0x40, t3 = 0xA4000040, ra = 0xA4001550

.n64
.create "boot.z64", 0
.word 0x80371240
.fill 0x3C
.headersize 0xA4000040-orga()
IPL3:
    addiu   $t2, $t3, @end-@start-4
@@loop:
    lw      $ra, (@start-0xA4000040)($t3)
    sw      $ra, (0xA4000000-0xA4000040)($t3)
    bne     $t3, $t2, @@loop
    addiu   $t3, 4
    lui     $t1, 0xA400
    lui     $t2, 0xB000
    jr      $t1
    addiu   $t3, $t2, 0xFC0
@start:
@@loop:
    lw      $ra, 0x1040($t2)
    sw      $ra, 0x0040($t1)
    addiu   $t2, 4
    bne     $t2, $t3, @@loop
    addiu   $t1, 4
    or      $t1, $0, $0
    addiu   $t2, $0, 0x40
    lui     $t3, 0xA400
    ori     $t3, 0x0040
    jr      $t3
    ori     $ra, $t3, 0x1550
@end:
.fill 0xA4001000-.
.close
