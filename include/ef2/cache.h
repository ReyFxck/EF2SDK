#ifndef EF2_CACHE_H
#define EF2_CACHE_H

#include <ef2/base.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Write back and invalidate every EE data-cache line intersecting the range.
 * This is the primitive used before EE -> DMAC transfers.
 */
void ef2_cache_writeback_invalidate_range(
    const void *ptr,
    ef2_u32 size);

#ifdef __cplusplus
}
#endif

#endif
