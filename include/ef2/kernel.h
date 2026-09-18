#ifndef EF2_KERNEL_H
#define EF2_KERNEL_H

#include <ef2/base.h>
#include <ef2/sif.h>


#define EF2_KERNEL_THREAD_SELF 0

enum {
    EF2_KERNEL_THREAD_RUN = 0x01,
    EF2_KERNEL_THREAD_READY = 0x02,
    EF2_KERNEL_THREAD_WAIT = 0x04,
    EF2_KERNEL_THREAD_SUSPEND = 0x08,
    EF2_KERNEL_THREAD_WAIT_SUSPEND = 0x0c,
    EF2_KERNEL_THREAD_DORMANT = 0x10
};

enum {
    EF2_KERNEL_THREAD_WAIT_NONE = 0,
    EF2_KERNEL_THREAD_WAIT_SLEEP = 1,
    EF2_KERNEL_THREAD_WAIT_SEMA = 2
};

typedef struct ef2_kernel_thread_param {
    ef2_s32 status;
    void *entry;
    void *stack;
    ef2_s32 stack_size;
    void *gp;
    ef2_s32 initial_priority;
    ef2_s32 current_priority;
    ef2_u32 attr;
    ef2_u32 option;
} ef2_kernel_thread_param;

typedef struct ef2_kernel_thread_status {
    ef2_s32 status;
    void *entry;
    void *stack;
    ef2_s32 stack_size;
    void *gp;
    ef2_s32 initial_priority;
    ef2_s32 current_priority;
    ef2_u32 attr;
    ef2_u32 option;
    ef2_u32 wait_type;
    ef2_u32 wait_id;
    ef2_u32 wakeup_count;
} ef2_kernel_thread_status;

typedef struct ef2_kernel_sema {
    ef2_s32 count;
    ef2_s32 max_count;
    ef2_s32 init_count;
    ef2_s32 wait_threads;
    ef2_u32 attr;
    ef2_u32 option;
} ef2_kernel_sema;

#ifdef __cplusplus
extern "C" {
#endif

void ef2_kernel_set_gs_crt(ef2_s16 interlace, ef2_s16 mode, ef2_s16 field_mode);
void ef2_kernel_exit(ef2_s32 status);

void ef2_kernel_set_vtlb_refill_handler(
    ef2_s32 handler_number,
    void *handler);

void ef2_kernel_set_v_common_handler(
    ef2_s32 handler_number,
    void *handler);
void ef2_kernel_setup_heap(void *start, ef2_s32 size);
void *ef2_kernel_end_of_heap(void);

ef2_s32 ef2_kernel_add_intc_handler2(
    ef2_s32 cause,
    ef2_s32 (*handler)(
        ef2_s32 cause,
        void *arg,
        void *address),
    ef2_s32 next,
    void *arg);
ef2_s32 ef2_kernel_remove_intc_handler(
    ef2_s32 cause,
    ef2_s32 handler_id);
ef2_s32 ef2_kernel_enable_intc(ef2_s32 cause);
ef2_s32 ef2_kernel_disable_intc(ef2_s32 cause);

ef2_s32 ef2_kernel_add_dmac_handler(
    ef2_s32 channel,
    ef2_s32 (*handler)(ef2_s32 channel),
    ef2_s32 next);
ef2_s32 ef2_kernel_add_dmac_handler2(
    ef2_s32 channel,
    ef2_s32 (*handler)(
        ef2_s32 channel,
        void *arg,
        void *address),
    ef2_s32 next,
    void *arg);
ef2_s32 ef2_kernel_remove_dmac_handler(
    ef2_s32 channel,
    ef2_s32 handler_id);
ef2_s32 ef2_kernel_enable_dmac(ef2_s32 channel);
ef2_s32 ef2_kernel_disable_dmac(ef2_s32 channel);
void ef2_kernel_flush_cache(ef2_s32 operation);

ef2_s32 ef2_kernel_create_thread(ef2_kernel_thread_param *thread);
ef2_s32 ef2_kernel_delete_thread(ef2_s32 thread_id);
ef2_s32 ef2_kernel_start_thread(ef2_s32 thread_id, void *arg);
void ef2_kernel_exit_thread(void);
void ef2_kernel_exit_delete_thread(void);
ef2_s32 ef2_kernel_terminate_thread(ef2_s32 thread_id);
ef2_s32 ef2_kernel_change_thread_priority(
    ef2_s32 thread_id,
    ef2_s32 priority);
ef2_s32 ef2_kernel_rotate_thread_ready_queue(ef2_s32 priority);
ef2_s32 ef2_kernel_release_wait_thread(ef2_s32 thread_id);
ef2_s32 ef2_kernel_get_thread_id(void);
ef2_s32 ef2_kernel_refer_thread_status(
    ef2_s32 thread_id,
    ef2_kernel_thread_status *status);
ef2_s32 ef2_kernel_sleep_thread(void);
ef2_s32 ef2_kernel_wakeup_thread(ef2_s32 thread_id);
ef2_s32 ef2_kernel_cancel_wakeup_thread(ef2_s32 thread_id);
ef2_s32 ef2_kernel_suspend_thread(ef2_s32 thread_id);
ef2_s32 ef2_kernel_resume_thread(ef2_s32 thread_id);

ef2_s32 ef2_kernel_create_sema(ef2_kernel_sema *sema);
ef2_s32 ef2_kernel_delete_sema(ef2_s32 sema_id);
ef2_s32 ef2_kernel_signal_sema(ef2_s32 sema_id);
ef2_s32 ef2_kernel_wait_sema(ef2_s32 sema_id);
ef2_s32 ef2_kernel_poll_sema(ef2_s32 sema_id);
ef2_s32 ef2_kernel_refer_sema_status(
    ef2_s32 sema_id,
    ef2_kernel_sema *sema);

ef2_s32 ef2_kernel_enable_cache(ef2_s32 cache_mask);
ef2_s32 ef2_kernel_disable_cache(ef2_s32 cache_mask);
ef2_u32 ef2_kernel_get_cop0(ef2_s32 register_id);
ef2_u32 ef2_kernel_cpu_config(ef2_u32 config);
ef2_s32 ef2_kernel_machine_type(void);
ef2_s32 ef2_kernel_get_memory_size(void);

ef2_s32 ef2_kernel_sif_dma_stat(ef2_s32 id);
ef2_s32 ef2_kernel_sif_set_dma(ef2_sif_dma_transfer *transfer, ef2_s32 count);
void ef2_kernel_sif_set_dchain(void);
void ef2_kernel_isif_set_dchain(void);
ef2_s32 ef2_kernel_sif_set_reg(ef2_u32 reg, ef2_s32 value);
ef2_s32 ef2_kernel_sif_get_reg(ef2_u32 reg);

#ifdef __cplusplus
}
#endif

#endif
