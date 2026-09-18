#include <ef2/timer.h>

#include <stdio.h>

static int expect(
    int condition,
    const char *name)
{
    if (condition)
        return 0;

    fprintf(stderr, "timer test failed: %s\n", name);
    return 1;
}

int main(void)
{
    int failed = 0;

    failed |= expect(
        ef2_timer_elapsed16(
            0xFFF0u,
            0x0010u) == 0x20u,
        "16-bit wrap");

    failed |= expect(
        ef2_cpu_count_elapsed(
            0xFFFFFFF0u,
            0x00000010u) == 0x20u,
        "32-bit wrap");

    failed |= expect(
        EF2_TIMER_BUS_CLOCK_HZ /
            EF2_TIMER_BUS_DIV16_HZ == 16u,
        "bus /16");

    failed |= expect(
        EF2_TIMER_BUS_CLOCK_HZ /
            EF2_TIMER_BUS_DIV256_HZ == 256u,
        "bus /256");

    return failed ? 1 : 0;
}
