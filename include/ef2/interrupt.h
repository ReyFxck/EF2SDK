#ifndef EF2_INTERRUPT_H
#define EF2_INTERRUPT_H

#include <ef2/base.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EF2_INTC_GS = 0,
    EF2_INTC_SBUS = 1,
    EF2_INTC_VBLANK_START = 2,
    EF2_INTC_VBLANK_END = 3,
    EF2_INTC_VIF0 = 4,
    EF2_INTC_VIF1 = 5,
    EF2_INTC_VU0 = 6,
    EF2_INTC_VU1 = 7,
    EF2_INTC_IPU = 8,
    EF2_INTC_TIMER0 = 9,
    EF2_INTC_TIMER1 = 10,
    EF2_INTC_TIMER2 = 11,
    EF2_INTC_SFIFO = 13,
    EF2_INTC_VU0_WATCHDOG = 14
} ef2_intc_source;

typedef enum {
    EF2_DMAC_VIF0 = 0,
    EF2_DMAC_VIF1 = 1,
    EF2_DMAC_GIF = 2,
    EF2_DMAC_FROM_IPU = 3,
    EF2_DMAC_TO_IPU = 4,
    EF2_DMAC_SIF0 = 5,
    EF2_DMAC_SIF1 = 6,
    EF2_DMAC_SIF2 = 7,
    EF2_DMAC_FROM_SPR = 8,
    EF2_DMAC_TO_SPR = 9,
    EF2_DMAC_CIS = 13,
    EF2_DMAC_MEIS = 14,
    EF2_DMAC_BEIS = 15
} ef2_dmac_source;

typedef ef2_s32 (*ef2_interrupt_handler)(
    ef2_s32 source,
    void *arg,
    void *address);

/*
 * Suspend global EE interrupt delivery and return whether it was enabled.
 * Pass the returned state to ef2_interrupt_resume().
 */
ef2_u32 ef2_interrupt_suspend(void);
void ef2_interrupt_resume(ef2_u32 state);

ef2_s32 ef2_interrupt_add_intc(
    ef2_intc_source source,
    ef2_interrupt_handler handler,
    void *arg,
    ef2_s32 next);

ef2_s32 ef2_interrupt_remove_intc(
    ef2_intc_source source,
    ef2_s32 handler_id);

ef2_s32 ef2_interrupt_enable_intc(
    ef2_intc_source source);

ef2_s32 ef2_interrupt_disable_intc(
    ef2_intc_source source);

ef2_s32 ef2_interrupt_add_dmac(
    ef2_dmac_source source,
    ef2_interrupt_handler handler,
    void *arg,
    ef2_s32 next);

ef2_s32 ef2_interrupt_remove_dmac(
    ef2_dmac_source source,
    ef2_s32 handler_id);

ef2_s32 ef2_interrupt_enable_dmac(
    ef2_dmac_source source);

ef2_s32 ef2_interrupt_disable_dmac(
    ef2_dmac_source source);

#ifdef __cplusplus
}
#endif

#endif
