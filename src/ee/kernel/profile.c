#include <ef2/profile.h>

void ef2_profile_reset(ef2_profile_counter *counter)
{
    if (counter == (ef2_profile_counter *)0)
        return;

    counter->samples = 0u;
    counter->last_ticks = 0u;
    counter->min_ticks = 0u;
    counter->max_ticks = 0u;
    counter->total_ticks = 0u;
}

void ef2_profile_record(
    ef2_profile_counter *counter,
    ef2_u32 ticks)
{
    if (counter == (ef2_profile_counter *)0)
        return;

    if (counter->samples == 0u) {
        counter->min_ticks = ticks;
        counter->max_ticks = ticks;
    } else {
        if (ticks < counter->min_ticks)
            counter->min_ticks = ticks;
        if (ticks > counter->max_ticks)
            counter->max_ticks = ticks;
    }

    counter->last_ticks = ticks;
    counter->total_ticks += (ef2_u64)ticks;
    ++counter->samples;
}
