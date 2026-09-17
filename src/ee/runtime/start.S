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

    jal     main
    nop

1:
    b       1b
    nop

    .end _start
