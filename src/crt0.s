@ GBA startup + cartridge header. The header checksum (0xBD) and logo are
@ patched/verified by tools/fix_header.py after linking.
    .syntax unified
    .section .header, "ax"
    .arm
    .global _start
_start:
    b       rom_start
    .space  156                 @ 0x04: Nintendo logo (filled by fix_header.py)
    .ascii  "GFAM"              @ 0xA0: title (12 bytes)
    .byte   0, 0, 0, 0, 0, 0, 0, 0
    .ascii  "ZGFM"              @ 0xAC: game code (must not collide with gpSP/mGBA per-game overrides)
    .ascii  "01"                @ 0xB0: maker code
    .byte   0x96                @ 0xB2: fixed value
    .byte   0                   @ 0xB3: main unit code
    .byte   0                   @ 0xB4: device type
    .space  7                   @ 0xB5: reserved
    .byte   0                   @ 0xBC: software version
    .byte   0                   @ 0xBD: complement check (patched)
    .byte   0, 0                @ 0xBE: reserved

rom_start:
    mov     r0, #0x12           @ IRQ mode stack
    msr     cpsr_cf, r0
    ldr     sp, =0x03007FA0
    mov     r0, #0x13           @ SVC mode stack
    msr     cpsr_cf, r0
    ldr     sp, =0x03007FE0
    mov     r0, #0x1F           @ System mode stack
    msr     cpsr_cf, r0
    ldr     sp, =0x03007F00

    ldr     r0, =__iwram_load   @ copy IWRAM image (code + data) from ROM
    ldr     r1, =__iwram_start
    ldr     r2, =__iwram_end
1:  cmp     r1, r2
    ldrcc   r3, [r0], #4
    strcc   r3, [r1], #4
    bcc     1b

    ldr     r0, =__bss_start    @ clear .bss
    ldr     r1, =__bss_end
    mov     r2, #0
2:  cmp     r0, r1
    strcc   r2, [r0], #4
    bcc     2b

    mrs     r0, cpsr            @ make sure IRQs are not masked (I bit clear)
    bic     r0, r0, #0x80
    msr     cpsr_c, r0
    ldr     r0, =main
    bx      r0
3:  b       3b
    .pool
