#ifndef EF2_HEAP_H
#define EF2_HEAP_H

#include <ef2/base.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    ef2_u32 total_bytes;
    ef2_u32 free_bytes;
    ef2_u32 largest_free_block;
    ef2_u32 active_allocations;
} ef2_heap_stats;

/*
 * Initialize the allocator over a caller-owned memory range.
 * The range is aligned inward to 16-byte boundaries.
 */
int ef2_heap_init(void *memory, ef2_u32 size);

/*
 * Initialize the default EE heap after the EF2SDK image/stack.
 * Uses the kernel SetupHeap/EndOfHeap pair only to establish the safe range.
 */
int ef2_heap_init_default(void);

int ef2_heap_is_ready(void);

void *ef2_malloc(ef2_u32 size);
void ef2_free(void *ptr);
void *ef2_calloc(ef2_u32 count, ef2_u32 size);
void *ef2_realloc(void *ptr, ef2_u32 size);

ef2_u32 ef2_heap_usable_size(const void *ptr);
int ef2_heap_get_stats(ef2_heap_stats *stats);

#ifdef __cplusplus
}
#endif

#endif
