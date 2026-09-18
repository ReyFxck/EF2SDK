#include <ef2/heap.h>

#define EF2_HEAP_ALIGNMENT 16u
#define EF2_HEAP_MAGIC_ALLOC 0xE2A110C1u
#define EF2_HEAP_MAGIC_FREE  0xE2F4EE01u

typedef unsigned long ef2_heap_uptr;

typedef struct ef2_heap_block {
    ef2_u32 size;
    ef2_u32 magic;
    struct ef2_heap_block *previous;
    struct ef2_heap_block *next;
} ef2_heap_block;

static ef2_heap_block *g_heap_first;
static ef2_u32 g_heap_total;
static ef2_u32 g_heap_active;
static int g_heap_ready;

static ef2_u32 align_up(ef2_u32 value)
{
    return (value + EF2_HEAP_ALIGNMENT - 1u) &
           ~(EF2_HEAP_ALIGNMENT - 1u);
}

static ef2_heap_uptr align_ptr_up(ef2_heap_uptr value)
{
    return (value + (ef2_heap_uptr)EF2_HEAP_ALIGNMENT - 1ul) &
           ~((ef2_heap_uptr)EF2_HEAP_ALIGNMENT - 1ul);
}

static ef2_heap_uptr align_ptr_down(ef2_heap_uptr value)
{
    return value &
           ~((ef2_heap_uptr)EF2_HEAP_ALIGNMENT - 1ul);
}

static ef2_u32 header_size(void)
{
    return align_up((ef2_u32)sizeof(ef2_heap_block));
}

static unsigned char *block_payload(ef2_heap_block *block)
{
    return (unsigned char *)block + header_size();
}

static ef2_heap_block *payload_block(const void *ptr)
{
    return (ef2_heap_block *)(
        (unsigned char *)ptr - header_size());
}

static void split_block(
    ef2_heap_block *block,
    ef2_u32 wanted)
{
    ef2_u32 hsize = header_size();
    ef2_u32 remaining;
    ef2_heap_block *split;

    if (block->size < wanted)
        return;

    remaining = block->size - wanted;

    if (remaining < hsize + EF2_HEAP_ALIGNMENT)
        return;

    split = (ef2_heap_block *)(
        block_payload(block) + wanted);

    split->size = remaining - hsize;
    split->magic = EF2_HEAP_MAGIC_FREE;
    split->previous = block;
    split->next = block->next;

    if (split->next != (ef2_heap_block *)0)
        split->next->previous = split;

    block->size = wanted;
    block->next = split;
}

static void merge_with_next(ef2_heap_block *block)
{
    ef2_heap_block *next;
    ef2_u32 hsize = header_size();

    if (block == (ef2_heap_block *)0)
        return;

    next = block->next;

    if (next == (ef2_heap_block *)0 ||
        next->magic != EF2_HEAP_MAGIC_FREE)
        return;

    block->size += hsize + next->size;
    block->next = next->next;

    if (block->next != (ef2_heap_block *)0)
        block->next->previous = block;
}

int ef2_heap_init(void *memory, ef2_u32 size)
{
    ef2_heap_uptr start;
    ef2_heap_uptr end;
    ef2_u32 hsize;
    ef2_heap_block *block;

    if (memory == (void *)0 || size == 0u)
        return -1;

    start = align_ptr_up((ef2_heap_uptr)memory);
    end = align_ptr_down(
        (ef2_heap_uptr)memory +
        (ef2_heap_uptr)size);
    hsize = header_size();

    if (end <= start ||
        end - start <
            (ef2_heap_uptr)(
                hsize + EF2_HEAP_ALIGNMENT))
        return -2;

    if (end - start > 0xFFFFFFFFul)
        return -3;

    block = (ef2_heap_block *)start;
    block->size =
        (ef2_u32)(end - start) - hsize;
    block->magic = EF2_HEAP_MAGIC_FREE;
    block->previous = (ef2_heap_block *)0;
    block->next = (ef2_heap_block *)0;

    g_heap_first = block;
    g_heap_total = block->size;
    g_heap_active = 0;
    g_heap_ready = 1;

    return 0;
}

int ef2_heap_is_ready(void)
{
    return g_heap_ready;
}

void *ef2_malloc(ef2_u32 size)
{
    ef2_heap_block *block;
    ef2_u32 wanted;

    if (!g_heap_ready || size == 0u)
        return (void *)0;

    if (size > 0xFFFFFFFFu -
               (EF2_HEAP_ALIGNMENT - 1u))
        return (void *)0;

    wanted = align_up(size);

    for (block = g_heap_first;
         block != (ef2_heap_block *)0;
         block = block->next) {
        if (block->magic != EF2_HEAP_MAGIC_FREE ||
            block->size < wanted)
            continue;

        split_block(block, wanted);
        block->magic = EF2_HEAP_MAGIC_ALLOC;
        ++g_heap_active;

        return block_payload(block);
    }

    return (void *)0;
}

void ef2_free(void *ptr)
{
    ef2_heap_block *block;

    if (!g_heap_ready || ptr == (void *)0)
        return;

    if (((ef2_heap_uptr)ptr &
         ((ef2_heap_uptr)EF2_HEAP_ALIGNMENT - 1ul)) != 0ul)
        return;

    block = payload_block(ptr);

    if (block->magic != EF2_HEAP_MAGIC_ALLOC)
        return;

    block->magic = EF2_HEAP_MAGIC_FREE;

    if (g_heap_active != 0u)
        --g_heap_active;

    merge_with_next(block);

    if (block->previous != (ef2_heap_block *)0 &&
        block->previous->magic ==
            EF2_HEAP_MAGIC_FREE) {
        block = block->previous;
        merge_with_next(block);
    }
}

void *ef2_calloc(ef2_u32 count, ef2_u32 size)
{
    ef2_u32 total;
    ef2_u32 i;
    unsigned char *ptr;

    if (count == 0u || size == 0u)
        return (void *)0;

    if (count > 0xFFFFFFFFu / size)
        return (void *)0;

    total = count * size;
    ptr = (unsigned char *)ef2_malloc(total);

    if (ptr == (unsigned char *)0)
        return (void *)0;

    for (i = 0; i < total; ++i)
        ptr[i] = 0;

    return ptr;
}

void *ef2_realloc(void *ptr, ef2_u32 size)
{
    ef2_heap_block *block;
    ef2_u32 wanted;
    ef2_u32 old_size;
    void *replacement;
    ef2_u32 copy_size;
    ef2_u32 i;

    if (ptr == (void *)0)
        return ef2_malloc(size);

    if (size == 0u) {
        ef2_free(ptr);
        return (void *)0;
    }

    if (!g_heap_ready ||
        ((ef2_heap_uptr)ptr &
         ((ef2_heap_uptr)EF2_HEAP_ALIGNMENT - 1ul)) != 0ul)
        return (void *)0;

    block = payload_block(ptr);

    if (block->magic != EF2_HEAP_MAGIC_ALLOC)
        return (void *)0;

    wanted = align_up(size);
    old_size = block->size;

    if (wanted <= old_size) {
        split_block(block, wanted);
        return ptr;
    }

    if (block->next != (ef2_heap_block *)0 &&
        block->next->magic == EF2_HEAP_MAGIC_FREE &&
        old_size + header_size() +
            block->next->size >= wanted) {
        merge_with_next(block);
        split_block(block, wanted);
        block->magic = EF2_HEAP_MAGIC_ALLOC;
        return ptr;
    }

    replacement = ef2_malloc(size);

    if (replacement == (void *)0)
        return (void *)0;

    copy_size = old_size < size
        ? old_size
        : size;

    for (i = 0; i < copy_size; ++i)
        ((unsigned char *)replacement)[i] =
            ((unsigned char *)ptr)[i];

    ef2_free(ptr);
    return replacement;
}

ef2_u32 ef2_heap_usable_size(const void *ptr)
{
    ef2_heap_block *block;

    if (!g_heap_ready || ptr == (const void *)0)
        return 0;

    block = payload_block(ptr);

    if (block->magic != EF2_HEAP_MAGIC_ALLOC)
        return 0;

    return block->size;
}

int ef2_heap_get_stats(ef2_heap_stats *stats)
{
    ef2_heap_block *block;
    ef2_u32 free_bytes = 0;
    ef2_u32 largest = 0;

    if (!g_heap_ready ||
        stats == (ef2_heap_stats *)0)
        return -1;

    for (block = g_heap_first;
         block != (ef2_heap_block *)0;
         block = block->next) {
        if (block->magic != EF2_HEAP_MAGIC_FREE)
            continue;

        free_bytes += block->size;

        if (block->size > largest)
            largest = block->size;
    }

    stats->total_bytes = g_heap_total;
    stats->free_bytes = free_bytes;
    stats->largest_free_block = largest;
    stats->active_allocations = g_heap_active;

    return 0;
}
