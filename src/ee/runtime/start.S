    .set noreorder
    .section .text.start, "ax", @progbits
    .align 4

    .globl _start
    .globl __start
    .ent _start

_start:
__start:
    la      $sp, __ef2_stack_top
    move    $fp, $zero

    /* A freestanding ELF must not assume the loader cleared .bss. */
    la      $8, __ef2_bss_start
    la      $9, __ef2_bss_end

.Lef2_zero_bss:
    beq     $8, $9, .Lef2_bss_done
    nop
    sb      $zero, 0($8)
    addiu   $8, $8, 1
    b       .Lef2_zero_bss
    nop

.Lef2_bss_done:
    /* $a0/$a1 are intentionally left untouched so a loader-provided
     * argc/argv can be handed to main once the public ABI is finalized. */
    jal     main
    nop

.Lef2_halt:
    b       .Lef2_halt
    nop

    .end _start
