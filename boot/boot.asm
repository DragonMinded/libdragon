; IPL3 trampoline code
; --------------------
;
; This is technically just a very small IPL3 which loads
; the real IPL3 from ROM from a fixed address (0x10001000).
; Basically, the ROM layout is:
;
;    Offset     Content
;   --------   ---------
;    0x0000     ROM header
;    0x0040     IPL3 trampoline code (must be signed)
;    0x1000     Real IPL3 (unsigned)
;    0x2000     Rest of ROM
;
; This trampoling is used to workaround the need of signing
; IPL3 during development. Being itself signed, it can run
; an unsigned piece of code and run it.
;
; Credits go to hcs64 for the IPL3 trampoline idea, and
; having first released it (https://github.com/hcs64/boot_stub).
;
; This trampoling is an improvement over the above, and
; was written by devwizard. It must be assembled with 
; armips (https://github.com/Kingcom/armips).
;
; This trampoline is completely transparent because it does
; restore the value of the CPU registers at the end of IPL2.
; 
; Register values at the end of IPL2:
; t1 = 0, t2 = 0x40, t3 = 0xA4000040, ra = 0xA4001550

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
    addiu   $t3, $sp, 0xA4000040-0xA4001FF0
    jr      $t3
    addiu   $ra, $sp, 0xA4001550-0xA4001FF0
@end:
.fill 0xA4001000-.
.close
