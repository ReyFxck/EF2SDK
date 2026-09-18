#include <ef2/timer.h>

#define EF2_TIMER_BASE(id) \
    (0x10000000u + ((ef2_u32)(id) * 0x800u))

#define EF2_TIMER_COUNT(id) \
    (*(volatile ef2_u32 *)(EF2_TIMER_BASE(id) + 0x00u))
#define EF2_TIMER_MODE(id) \
    (*(volatile ef2_u32 *)(EF2_TIMER_BASE(id) + 0x10u))
#define EF2_TIMER_COMP(id) \
    (*(volatile ef2_u32 *)(EF2_TIMER_BASE(id) + 0x20u))

#define EF2_TIMER_MODE_CLOCK_MASK 0x00000003u
#define EF2_TIMER_MODE_ZERO_RETURN 0x00000040u
#define EF2_TIMER_MODE_ENABLE 0x00000080u
#define EF2_TIMER_MODE_COMPARE_IRQ 0x00000100u
#define EF2_TIMER_MODE_OVERFLOW_IRQ 0x00000200u
#define EF2_TIMER_MODE_COMPARE_FLAG 0x00000400u
#define EF2_TIMER_MODE_OVERFLOW_FLAG 0x00000800u
#define EF2_TIMER_MODE_EVENT_MASK \
    (EF2_TIMER_MODE_COMPARE_FLAG | \
     EF2_TIMER_MODE_OVERFLOW_FLAG)

static int ef2_timer_valid(ef2_timer_id timer)
{
    return timer >= EF2_TIMER_0 &&
           timer <= EF2_TIMER_2;
}

static ef2_u32 ef2_timer_mode_control(
    ef2_u32 mode)
{
    return mode & ~EF2_TIMER_MODE_EVENT_MASK;
}

int ef2_timer_configure(
    ef2_timer_id timer,
    const ef2_timer_config *config)
{
    ef2_u32 mode;

    if (!ef2_timer_valid(timer) ||
        config == (const ef2_timer_config *)0)
        return -1;

    if (config->clock < EF2_TIMER_CLOCK_BUS ||
        config->clock > EF2_TIMER_CLOCK_HBLANK)
        return -2;

    /*
     * Timer 2 is used as a conventional bus-clock timer here. HBLANK
     * clocking is kept to timers 0/1, matching established PS2 usage.
     */
    if (timer == EF2_TIMER_2 &&
        config->clock == EF2_TIMER_CLOCK_HBLANK)
        return -3;

    mode =
        ((ef2_u32)config->clock &
         EF2_TIMER_MODE_CLOCK_MASK);

    if ((config->flags &
         EF2_TIMER_FLAG_ZERO_ON_COMPARE) != 0u)
        mode |= EF2_TIMER_MODE_ZERO_RETURN;

    if ((config->flags &
         EF2_TIMER_FLAG_IRQ_COMPARE) != 0u)
        mode |= EF2_TIMER_MODE_COMPARE_IRQ;

    if ((config->flags &
         EF2_TIMER_FLAG_IRQ_OVERFLOW) != 0u)
        mode |= EF2_TIMER_MODE_OVERFLOW_IRQ;

    EF2_TIMER_COUNT(timer) = 0;
    EF2_TIMER_COMP(timer) = config->compare;

    /*
     * Status bits are write-one-to-clear. Set both here so configuration
     * begins with no stale compare/overflow condition.
     */
    EF2_TIMER_MODE(timer) =
        mode |
        EF2_TIMER_MODE_COMPARE_FLAG |
        EF2_TIMER_MODE_OVERFLOW_FLAG;

    __asm__ volatile("sync.p" ::: "memory");
    return 0;
}

int ef2_timer_start(ef2_timer_id timer)
{
    ef2_u32 mode;

    if (!ef2_timer_valid(timer))
        return -1;

    mode =
        ef2_timer_mode_control(
            EF2_TIMER_MODE(timer));

    mode |= EF2_TIMER_MODE_ENABLE;
    EF2_TIMER_MODE(timer) = mode;

    __asm__ volatile("sync.p" ::: "memory");
    return 0;
}

int ef2_timer_stop(ef2_timer_id timer)
{
    ef2_u32 mode;

    if (!ef2_timer_valid(timer))
        return -1;

    mode =
        ef2_timer_mode_control(
            EF2_TIMER_MODE(timer));

    mode &= ~EF2_TIMER_MODE_ENABLE;
    EF2_TIMER_MODE(timer) = mode;

    __asm__ volatile("sync.p" ::: "memory");
    return 0;
}

int ef2_timer_set_count(
    ef2_timer_id timer,
    ef2_u16 count)
{
    if (!ef2_timer_valid(timer))
        return -1;

    EF2_TIMER_COUNT(timer) = count;
    __asm__ volatile("sync.p" ::: "memory");
    return 0;
}

ef2_u16 ef2_timer_get_count(
    ef2_timer_id timer)
{
    if (!ef2_timer_valid(timer))
        return 0;

    return (ef2_u16)EF2_TIMER_COUNT(timer);
}

ef2_u16 ef2_timer_get_compare(
    ef2_timer_id timer)
{
    if (!ef2_timer_valid(timer))
        return 0;

    return (ef2_u16)EF2_TIMER_COMP(timer);
}

ef2_u32 ef2_timer_get_events(
    ef2_timer_id timer)
{
    ef2_u32 mode;
    ef2_u32 events = 0;

    if (!ef2_timer_valid(timer))
        return 0;

    mode = EF2_TIMER_MODE(timer);

    if ((mode &
         EF2_TIMER_MODE_COMPARE_FLAG) != 0u)
        events |= EF2_TIMER_EVENT_COMPARE;

    if ((mode &
         EF2_TIMER_MODE_OVERFLOW_FLAG) != 0u)
        events |= EF2_TIMER_EVENT_OVERFLOW;

    return events;
}

int ef2_timer_ack(
    ef2_timer_id timer,
    ef2_u32 events)
{
    ef2_u32 mode;

    if (!ef2_timer_valid(timer))
        return -1;

    mode =
        ef2_timer_mode_control(
            EF2_TIMER_MODE(timer));

    if ((events &
         EF2_TIMER_EVENT_COMPARE) != 0u)
        mode |= EF2_TIMER_MODE_COMPARE_FLAG;

    if ((events &
         EF2_TIMER_EVENT_OVERFLOW) != 0u)
        mode |= EF2_TIMER_MODE_OVERFLOW_FLAG;

    EF2_TIMER_MODE(timer) = mode;
    __asm__ volatile("sync.p" ::: "memory");

    return 0;
}

ef2_u32 ef2_timer_clock_hz(
    ef2_timer_clock clock,
    ef2_u32 hblank_hz)
{
    switch (clock) {
        case EF2_TIMER_CLOCK_BUS:
            return EF2_TIMER_BUS_CLOCK_HZ;

        case EF2_TIMER_CLOCK_BUS_DIV16:
            return EF2_TIMER_BUS_DIV16_HZ;

        case EF2_TIMER_CLOCK_BUS_DIV256:
            return EF2_TIMER_BUS_DIV256_HZ;

        case EF2_TIMER_CLOCK_HBLANK:
            return hblank_hz;

        default:
            return 0;
    }
}

ef2_u32 ef2_cpu_count(void)
{
    ef2_u32 count;

    __asm__ volatile(
        "mfc0 %0, $9"
        : "=r"(count));

    return count;
}
