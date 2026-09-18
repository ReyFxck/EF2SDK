#include <ef2/heap.h>

#include <stdint.h>
#include <stdio.h>

static unsigned char heap_area[8192]
    __attribute__((aligned(64)));

static int expect(int condition, const char *name)
{
    if (condition)
        return 0;

    fprintf(stderr, "heap test failed: %s\n", name);
    return 1;
}

int main(void)
{
    ef2_heap_stats stats;
    unsigned char *a;
    unsigned char *b;
    unsigned char *c;
    unsigned char *d;
    ef2_u32 i;
    int failed = 0;

    failed |= expect(
        ef2_heap_init(heap_area, sizeof(heap_area)) == 0,
        "init");

    a = (unsigned char *)ef2_malloc(100);
    b = (unsigned char *)ef2_malloc(200);
    c = (unsigned char *)ef2_calloc(32, 4);

    failed |= expect(a != NULL, "malloc a");
    failed |= expect(b != NULL, "malloc b");
    failed |= expect(c != NULL, "calloc c");

    failed |= expect(
        ((uintptr_t)a & 15u) == 0u &&
        ((uintptr_t)b & 15u) == 0u &&
        ((uintptr_t)c & 15u) == 0u,
        "16-byte alignment");

    for (i = 0; i < 128u; ++i)
        failed |= expect(c[i] == 0u, "calloc zero");

    for (i = 0; i < 100u; ++i)
        a[i] = (unsigned char)(i ^ 0x5Au);

    a = (unsigned char *)ef2_realloc(a, 320);
    failed |= expect(a != NULL, "realloc grow");

    for (i = 0; i < 100u; ++i)
        failed |= expect(
            a[i] == (unsigned char)(i ^ 0x5Au),
            "realloc preserve");

    a = (unsigned char *)ef2_realloc(a, 48);
    failed |= expect(a != NULL, "realloc shrink");
    failed |= expect(
        ef2_heap_usable_size(a) >= 48u,
        "usable size");

    ef2_free(b);
    ef2_free(c);
    ef2_free(a);

    failed |= expect(
        ef2_heap_get_stats(&stats) == 0,
        "stats");
    failed |= expect(
        stats.active_allocations == 0u,
        "no active allocations");

    d = (unsigned char *)ef2_malloc(7000);
    failed |= expect(
        d != NULL,
        "coalesced large allocation");

    ef2_free(d);

    failed |= expect(
        ef2_calloc(0xFFFFFFFFu, 8u) == NULL,
        "calloc overflow");

    return failed ? 1 : 0;
}
