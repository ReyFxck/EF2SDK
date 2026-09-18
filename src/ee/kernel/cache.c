#include <ef2/cache.h>

void ef2_cache_writeback_invalidate_range(
    const void *ptr,
    ef2_u32 size)
{
    ef2_u32 address;
    ef2_u32 end;

    if (ptr == (const void *)0 || size == 0u)
        return;

    address = (ef2_u32)ptr & ~63u;
    end =
        ((ef2_u32)ptr + size + 63u) &
        ~63u;

    while (address < end) {
        __asm__ volatile(
            "sync\n\t"
            "cache 0x18, 0(%0)\n\t"
            "sync\n\t"
            :
            : "r"(address)
            : "memory");

        address += 64u;
    }
}
