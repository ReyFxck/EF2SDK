    .set noreorder
    .section .text.start, "ax", @progbits
    .align 4

    .globl _start
    .globl __start
    .ent _start

_start:
__start:
    /*
     * Preserve the loader-provided argument registers before touching BSS.
     * Different launchers use either argc/argv directly or a pointer to an
     * sargs-style structure; runtime.c normalizes both forms.
     */
    move    $s0, $a0
    move    $s1, $a1

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
    move    $a0, $s0
    move    $a1, $s1
    jal     ef2_runtime_capture_loader_args
    nop

    jal     ef2_runtime_run_main
    nop

.Lef2_halt:
    b       .Lef2_halt
    nop

    .end _start
