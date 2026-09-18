#include "crash_defs.h"

    .set noreorder
    .section .text, "ax", @progbits
    .align 4

    .globl ef2_crash_vector
    .type ef2_crash_vector, @function
    .ent ef2_crash_vector

ef2_crash_vector:
    /* k0 is scratch in level-1 exceptions; preserve every other GPR low 64. */
    lui     $k0, %hi(g_ef2_crash_frame)
    addiu   $k0, $k0, %lo(g_ef2_crash_frame)

    sd      $zero,   0($k0)
.set push
.set noat
    sd      $at,     8($k0)
.set pop
    sd      $v0,    16($k0)
    sd      $v1,    24($k0)
    sd      $a0,    32($k0)
    sd      $a1,    40($k0)
    sd      $a2,    48($k0)
    sd      $a3,    56($k0)
    sd      $t0,    64($k0)
    sd      $t1,    72($k0)
    sd      $t2,    80($k0)
    sd      $t3,    88($k0)
    sd      $t4,    96($k0)
    sd      $t5,   104($k0)
    sd      $t6,   112($k0)
    sd      $t7,   120($k0)
    sd      $s0,   128($k0)
    sd      $s1,   136($k0)
    sd      $s2,   144($k0)
    sd      $s3,   152($k0)
    sd      $s4,   160($k0)
    sd      $s5,   168($k0)
    sd      $s6,   176($k0)
    sd      $s7,   184($k0)
    sd      $t8,   192($k0)
    sd      $t9,   200($k0)
    sd      $zero, 208($k0)
    sd      $k1,   216($k0)
    sd      $gp,   224($k0)
    sd      $sp,   232($k0)
    sd      $fp,   240($k0)
    sd      $ra,   248($k0)

    mfc0    $k1, $12
    sw      $k1, EF2_CRASH_FRAME_STATUS($k0)
    mfc0    $k1, $13
    sw      $k1, EF2_CRASH_FRAME_CAUSE($k0)
    mfc0    $k1, $14
    sw      $k1, EF2_CRASH_FRAME_EPC($k0)
    mfc0    $k1, $30
    sw      $k1, EF2_CRASH_FRAME_ERROREPC($k0)
    mfc0    $k1, $8
    sw      $k1, EF2_CRASH_FRAME_BADVADDR($k0)

    lui     $sp, %hi(g_ef2_crash_stack_top)
    addiu   $sp, $sp, %lo(g_ef2_crash_stack_top)

    move    $a0, $k0
    jal     ef2_crash_dispatch
    nop

1:
    b       1b
    nop

    .end ef2_crash_vector
    .size ef2_crash_vector, .-ef2_crash_vector

    .globl ef2_crash_trigger_test
    .type ef2_crash_trigger_test, @function
    .ent ef2_crash_trigger_test

ef2_crash_trigger_test:
    /* Trap exception (ExcCode 13) without dereferencing invalid memory. */
    teq     $zero, $zero
    nop
2:
    b       2b
    nop

    .end ef2_crash_trigger_test
    .size ef2_crash_trigger_test, .-ef2_crash_trigger_test

    .section .bss, "aw", @nobits
    .align 7

g_ef2_crash_frame:
    .space EF2_CRASH_FRAME_SIZE

    .align 7
g_ef2_crash_stack:
    .space EF2_CRASH_STACK_BYTES
g_ef2_crash_stack_top:
