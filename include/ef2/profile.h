#ifndef EF2_PROFILE_H
#define EF2_PROFILE_H

#include <ef2/base.h>
#include <ef2/timer.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    ef2_u32 samples;
    ef2_u32 last_ticks;
    ef2_u32 min_ticks;
    ef2_u32 max_ticks;
    ef2_u64 total_ticks;
} ef2_profile_counter;

void ef2_profile_reset(ef2_profile_counter *counter);
void ef2_profile_record(
    ef2_profile_counter *counter,
    ef2_u32 ticks);

static inline ef2_u32 ef2_profile_begin(void)
{
    return ef2_cpu_count();
}

static inline ef2_u32 ef2_profile_end(
    ef2_profile_counter *counter,
    ef2_u32 start)
{
    ef2_u32 ticks =
        ef2_cpu_count_elapsed(
            start,
            ef2_cpu_count());

    ef2_profile_record(counter, ticks);
    return ticks;
}

#ifdef __cplusplus
}
#endif

#endif
