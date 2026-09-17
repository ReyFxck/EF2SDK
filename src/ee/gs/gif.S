    .set noreorder
    .section .text, "ax", @progbits
    .align 4

    .globl ef2_gif_send_qwords
    .type ef2_gif_send_qwords, @function
    .ent ef2_gif_send_qwords

ef2_gif_send_qwords:
    beqz    $5, 2f
    lui     $8, 0x1000
    ori     $8, $8, 0x6000

1:
    lq      $9, 0($4)
    sq      $9, 0($8)
    addiu   $5, $5, -1
    bnez    $5, 1b
    addiu   $4, $4, 16

2:
    sync.p
    jr      $31
    nop

    .end ef2_gif_send_qwords
    .size ef2_gif_send_qwords, .-ef2_gif_send_qwords
