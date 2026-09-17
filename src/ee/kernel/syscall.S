    .set noreorder
    .section .text, "ax", @progbits
    .align 4

    .globl ef2_kernel_set_gs_crt
    .type ef2_kernel_set_gs_crt, @function
    .ent ef2_kernel_set_gs_crt

ef2_kernel_set_gs_crt:
    li      $3, 2
    syscall
    jr      $31
    nop

    .end ef2_kernel_set_gs_crt
    .size ef2_kernel_set_gs_crt, .-ef2_kernel_set_gs_crt
