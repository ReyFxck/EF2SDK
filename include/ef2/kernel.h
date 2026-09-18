#ifndef EF2_KERNEL_H
#define EF2_KERNEL_H

#include <ef2/base.h>
#include <ef2/sif.h>

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
