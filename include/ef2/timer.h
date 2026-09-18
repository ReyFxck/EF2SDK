#ifndef EF2_TIMER_H
#define EF2_TIMER_H

#include <ef2/base.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EF2_TIMER_BUS_CLOCK_HZ 147456000u
#define EF2_TIMER_BUS_DIV16_HZ 9216000u
#define EF2_TIMER_BUS_DIV256_HZ 576000u
#define EF2_TIMER_HBLANK_NTSC_HZ 15734u
#define EF2_TIMER_HBLANK_PAL_HZ 15625u

typedef enum {
    EF2_TIMER_0 = 0,
    EF2_TIMER_1 = 1,
    EF2_TIMER_2 = 2
} ef2_timer_id;

typedef enum {
    EF2_TIMER_CLOCK_BUS = 0,
    EF2_TIMER_CLOCK_BUS_DIV16 = 1,
    EF2_TIMER_CLOCK_BUS_DIV256 = 2,
    EF2_TIMER_CLOCK_HBLANK = 3
} ef2_timer_clock;

enum {
    EF2_TIMER_FLAG_ZERO_ON_COMPARE = 1u << 0,
    EF2_TIMER_FLAG_IRQ_COMPARE = 1u << 1,
    EF2_TIMER_FLAG_IRQ_OVERFLOW = 1u << 2
};

enum {
    EF2_TIMER_EVENT_COMPARE = 1u << 0,
    EF2_TIMER_EVENT_OVERFLOW = 1u << 1
};

typedef struct {
    ef2_timer_clock clock;
    ef2_u16 compare;
    ef2_u32 flags;
} ef2_timer_config;

int ef2_timer_configure(
    ef2_timer_id timer,
    const ef2_timer_config *config);

int ef2_timer_start(ef2_timer_id timer);
int ef2_timer_stop(ef2_timer_id timer);

int ef2_timer_set_count(
    ef2_timer_id timer,
    ef2_u16 count);

ef2_u16 ef2_timer_get_count(
    ef2_timer_id timer);

ef2_u16 ef2_timer_get_compare(
    ef2_timer_id timer);

ef2_u32 ef2_timer_get_events(
    ef2_timer_id timer);

int ef2_timer_ack(
    ef2_timer_id timer,
    ef2_u32 events);

ef2_u32 ef2_timer_clock_hz(
    ef2_timer_clock clock,
    ef2_u32 hblank_hz);

/* Handles one 16-bit timer wrap naturally. */
static inline ef2_u16 ef2_timer_elapsed16(
    ef2_u16 start,
    ef2_u16 end)
{
    return (ef2_u16)(end - start);
}

/* R5900 COP0 Count, nominally half the 294.912 MHz CPU clock. */
ef2_u32 ef2_cpu_count(void);

/* Handles one 32-bit COP0 Count wrap naturally. */
static inline ef2_u32 ef2_cpu_count_elapsed(
    ef2_u32 start,
    ef2_u32 end)
{
    return end - start;
}

#ifdef __cplusplus
}
#endif

#endif
