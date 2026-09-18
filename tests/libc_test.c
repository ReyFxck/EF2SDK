#include <ef2/libc.h>

#include <stdio.h>

static int expect(int condition, const char *name)
{
    if (condition)
        return 0;

    fprintf(stderr, "libc test failed: %s\n", name);
    return 1;
}

int main(void)
{
    unsigned char a[32];
    unsigned char b[32];
    char text[32];
    int failed = 0;
    ef2_u32 i;

    ef2_memset(a, 0x5A, sizeof(a));

    for (i = 0; i < sizeof(a); ++i)
        failed |= expect(
            a[i] == 0x5Au,
            "memset");

    ef2_memset(b, 0, sizeof(b));
    ef2_memcpy(b, a, sizeof(a));

    failed |= expect(
        ef2_memcmp(a, b, sizeof(a)) == 0,
        "memcpy/memcmp equal");

    b[17] = 0x60;
    failed |= expect(
        ef2_memcmp(a, b, sizeof(a)) < 0,
        "memcmp ordering");

    for (i = 0; i < 16u; ++i)
        a[i] = (unsigned char)i;

    ef2_memmove(a + 4, a, 12);

    for (i = 0; i < 12u; ++i)
        failed |= expect(
            a[4u + i] == (unsigned char)i,
            "memmove overlap backward");

    ef2_memmove(a, a + 4, 12);

    for (i = 0; i < 12u; ++i)
        failed |= expect(
            a[i] == (unsigned char)i,
            "memmove overlap forward");

    ef2_strcpy(text, "EF2SDK");
    failed |= expect(
        ef2_strlen(text) == 6u,
        "strlen");
    failed |= expect(
        ef2_strcmp(text, "EF2SDK") == 0,
        "strcmp equal");
    failed |= expect(
        ef2_strcmp("ABC", "ABD") < 0,
        "strcmp order");
    failed |= expect(
        ef2_strncmp("alpha31", "alpha32", 5) == 0,
        "strncmp prefix");

    ef2_memset(text, 0x7F, sizeof(text));
    ef2_strncpy(text, "EF2", 8);

    failed |= expect(
        text[0] == 'E' &&
        text[1] == 'F' &&
        text[2] == '2' &&
        text[3] == '\0' &&
        text[7] == '\0',
        "strncpy zero fill");

    return failed ? 1 : 0;
}
