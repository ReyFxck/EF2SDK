#include <ef2/profile.h>

#include <stdio.h>

static int expect(
    int condition,
    const char *name)
{
    if (condition)
        return 0;

    fprintf(stderr, "profile test failed: %s\n", name);
    return 1;
}

int main(void)
{
    ef2_profile_counter counter;
    int failed = 0;

    ef2_profile_reset(&counter);

    failed |= expect(counter.samples == 0u, "reset samples");
    failed |= expect(counter.total_ticks == 0u, "reset total");

    ef2_profile_record(&counter, 40u);
    ef2_profile_record(&counter, 10u);
    ef2_profile_record(&counter, 70u);

    failed |= expect(counter.samples == 3u, "sample count");
    failed |= expect(counter.last_ticks == 70u, "last");
    failed |= expect(counter.min_ticks == 10u, "min");
    failed |= expect(counter.max_ticks == 70u, "max");
    failed |= expect(counter.total_ticks == 120u, "total");

    ef2_profile_reset((ef2_profile_counter *)0);
    ef2_profile_record((ef2_profile_counter *)0, 1u);

    return failed ? 1 : 0;
}
