    .set noreorder
    .section .text, "ax", @progbits
    .align 4

    .macro EF2_SYSCALL name, number
    .globl \name
    .type \name, @function
    .ent \name
\name:
    li      $3, \number
    syscall
    jr      $31
    nop
    .end \name
    .size \name, .-\name
    .endm

    EF2_SYSCALL ef2_kernel_set_gs_crt, 2
    EF2_SYSCALL ef2_kernel_add_dmac_handler, 0x12
    EF2_SYSCALL ef2_kernel_enable_dmac, 0x16
    EF2_SYSCALL ef2_kernel_flush_cache, 0x64
    EF2_SYSCALL ef2_kernel_sif_dma_stat, 0x76
    EF2_SYSCALL ef2_kernel_sif_set_dma, 0x77
    EF2_SYSCALL ef2_kernel_sif_set_dchain, 0x78
    EF2_SYSCALL ef2_kernel_isif_set_dchain, -0x78
    EF2_SYSCALL ef2_kernel_sif_set_reg, 0x79
    EF2_SYSCALL ef2_kernel_sif_get_reg, 0x7a
