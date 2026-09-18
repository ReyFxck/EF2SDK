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
    EF2_SYSCALL ef2_kernel_exit, 4
    EF2_SYSCALL ef2_kernel_set_vtlb_refill_handler, 0x0d
    EF2_SYSCALL ef2_kernel_set_v_common_handler, 0x0e
    EF2_SYSCALL ef2_kernel_setup_heap, 0x3d
    EF2_SYSCALL ef2_kernel_end_of_heap, 0x3e
    EF2_SYSCALL ef2_kernel_add_intc_handler2, 0x10
    EF2_SYSCALL ef2_kernel_remove_intc_handler, 0x11
    EF2_SYSCALL ef2_kernel_add_dmac_handler, 0x12
    EF2_SYSCALL ef2_kernel_add_dmac_handler2, 0x12
    EF2_SYSCALL ef2_kernel_remove_dmac_handler, 0x13
    EF2_SYSCALL ef2_kernel_enable_intc, 0x14
    EF2_SYSCALL ef2_kernel_disable_intc, 0x15
    EF2_SYSCALL ef2_kernel_enable_dmac, 0x16
    EF2_SYSCALL ef2_kernel_disable_dmac, 0x17
    EF2_SYSCALL ef2_kernel_flush_cache, 0x64
    EF2_SYSCALL ef2_kernel_sif_dma_stat, 0x76
    EF2_SYSCALL ef2_kernel_sif_set_dma, 0x77
    EF2_SYSCALL ef2_kernel_sif_set_dchain, 0x78
    EF2_SYSCALL ef2_kernel_isif_set_dchain, -0x78
    EF2_SYSCALL ef2_kernel_sif_set_reg, 0x79
    EF2_SYSCALL ef2_kernel_sif_get_reg, 0x7a

    EF2_SYSCALL ef2_kernel_create_thread, 0x20
    EF2_SYSCALL ef2_kernel_delete_thread, 0x21
    EF2_SYSCALL ef2_kernel_start_thread, 0x22
    EF2_SYSCALL ef2_kernel_exit_thread, 0x23
    EF2_SYSCALL ef2_kernel_exit_delete_thread, 0x24
    EF2_SYSCALL ef2_kernel_terminate_thread, 0x25
    EF2_SYSCALL ef2_kernel_change_thread_priority, 0x29
    EF2_SYSCALL ef2_kernel_rotate_thread_ready_queue, 0x2b
    EF2_SYSCALL ef2_kernel_release_wait_thread, 0x2d
    EF2_SYSCALL ef2_kernel_get_thread_id, 0x2f
    EF2_SYSCALL ef2_kernel_refer_thread_status, 0x30
    EF2_SYSCALL ef2_kernel_sleep_thread, 0x32
    EF2_SYSCALL ef2_kernel_wakeup_thread, 0x33
    EF2_SYSCALL ef2_kernel_cancel_wakeup_thread, 0x35
    EF2_SYSCALL ef2_kernel_suspend_thread, 0x37
    EF2_SYSCALL ef2_kernel_resume_thread, 0x39

    EF2_SYSCALL ef2_kernel_create_sema, 0x40
    EF2_SYSCALL ef2_kernel_delete_sema, 0x41
    EF2_SYSCALL ef2_kernel_signal_sema, 0x42
    EF2_SYSCALL ef2_kernel_wait_sema, 0x44
    EF2_SYSCALL ef2_kernel_poll_sema, 0x45
    EF2_SYSCALL ef2_kernel_refer_sema_status, 0x47

    EF2_SYSCALL ef2_kernel_enable_cache, 0x61
    EF2_SYSCALL ef2_kernel_disable_cache, 0x62
    EF2_SYSCALL ef2_kernel_get_cop0, 0x63
    EF2_SYSCALL ef2_kernel_cpu_config, 0x66
    EF2_SYSCALL ef2_kernel_machine_type, 0x7e
    EF2_SYSCALL ef2_kernel_get_memory_size, 0x7f
