#include <ef2/libc.h>
#include <stdio.h>

static int same_text(
    const char *left,
    const char *right)
{
    while (*left != '\0' &&
           *left == *right) {
        ++left;
        ++right;
    }

    return *left == *right;
}

static int test_namespaced(void)
{
    char buffer[128];
    int result;

    result = ef2_snprintf(
        buffer,
        sizeof(buffer),
        "%s %u %d %08x %X %%",
        "EF2",
        37u,
        -42,
        42u,
        0xBEEFu);

    if (result != 26 ||
        !same_text(
            buffer,
            "EF2 37 -42 0000002a BEEF %"))
        return 1;

    result = ef2_snprintf(
        buffer,
        sizeof(buffer),
        "%08d %llu %lld",
        -42,
        1234567890123ull,
        -1234567890123ll);

    if (result != 37 ||
        !same_text(
            buffer,
            "-0000042 1234567890123 -1234567890123"))
        return 2;

    result = ef2_snprintf(
        buffer,
        sizeof(buffer),
        "%3c %8s %zu",
        'x',
        "abc",
        (ef2_size_t)99u);

    if (result != 15 ||
        !same_text(
            buffer,
            "  x      abc 99"))
        return 3;

    return 0;
}

static int test_standard_alias(void)
{
    char buffer[5];
    int result;

    result = snprintf(
        buffer,
        sizeof(buffer),
        "abcdef");

    if (result != 6 ||
        !same_text(buffer, "abcd"))
        return 4;

    result = snprintf(
        (char *)0,
        0,
        "%s:%u",
        "EF2",
        35u);

    if (result != 6)
        return 5;

    return 0;
}

int main(void)
{
    int result;

    result = test_namespaced();
    if (result != 0)
        return result;

    return test_standard_alias();
}
