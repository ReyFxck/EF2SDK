#include <ef2/heap.h>
#include <ef2/libc.h>

#ifndef EF2_LIBC_NO_STANDARD_ALIASES

static int size_fits_ee(ef2_size_t size)
{
#if __SIZEOF_SIZE_T__ > 4
    return size <=
        (ef2_size_t)0xFFFFFFFFu;
#else
    (void)size;
    return 1;
#endif
}

void *malloc(ef2_size_t size)
{
    if (!size_fits_ee(size))
        return (void *)0;

    return ef2_malloc((ef2_u32)size);
}

void free(void *ptr)
{
    ef2_free(ptr);
}

void *calloc(
    ef2_size_t count,
    ef2_size_t size)
{
    if (!size_fits_ee(count) ||
        !size_fits_ee(size))
        return (void *)0;

    return ef2_calloc(
        (ef2_u32)count,
        (ef2_u32)size);
}

void *realloc(
    void *ptr,
    ef2_size_t size)
{
    if (!size_fits_ee(size))
        return (void *)0;

    return ef2_realloc(
        ptr,
        (ef2_u32)size);
}

#endif
