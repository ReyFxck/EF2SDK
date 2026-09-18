#include <ef2/libc.h>

void *ef2_memcpy(
    void *destination,
    const void *source,
    ef2_size_t size)
{
    unsigned char *dst =
        (unsigned char *)destination;
    const unsigned char *src =
        (const unsigned char *)source;
    ef2_size_t i;

    for (i = 0; i < size; ++i)
        dst[i] = src[i];

    return destination;
}

void *ef2_memmove(
    void *destination,
    const void *source,
    ef2_size_t size)
{
    unsigned char *dst =
        (unsigned char *)destination;
    const unsigned char *src =
        (const unsigned char *)source;

    if (dst == src || size == 0u)
        return destination;

    if (dst < src) {
        ef2_size_t i;

        for (i = 0; i < size; ++i)
            dst[i] = src[i];
    } else {
        ef2_size_t i = size;

        while (i != 0u) {
            --i;
            dst[i] = src[i];
        }
    }

    return destination;
}

void *ef2_memset(
    void *destination,
    int value,
    ef2_size_t size)
{
    unsigned char *dst =
        (unsigned char *)destination;
    unsigned char byte =
        (unsigned char)value;
    ef2_size_t i;

    for (i = 0; i < size; ++i)
        dst[i] = byte;

    return destination;
}

int ef2_memcmp(
    const void *left,
    const void *right,
    ef2_size_t size)
{
    const unsigned char *a =
        (const unsigned char *)left;
    const unsigned char *b =
        (const unsigned char *)right;
    ef2_size_t i;

    for (i = 0; i < size; ++i) {
        if (a[i] < b[i])
            return -1;
        if (a[i] > b[i])
            return 1;
    }

    return 0;
}

ef2_size_t ef2_strlen(const char *text)
{
    ef2_size_t size = 0;

    while (text[size] != '\0')
        ++size;

    return size;
}

int ef2_strcmp(
    const char *left,
    const char *right)
{
    ef2_size_t i = 0;

    for (;;) {
        unsigned char a =
            (unsigned char)left[i];
        unsigned char b =
            (unsigned char)right[i];

        if (a < b)
            return -1;
        if (a > b)
            return 1;
        if (a == 0u)
            return 0;

        ++i;
    }
}

int ef2_strncmp(
    const char *left,
    const char *right,
    ef2_size_t size)
{
    ef2_size_t i;

    for (i = 0; i < size; ++i) {
        unsigned char a =
            (unsigned char)left[i];
        unsigned char b =
            (unsigned char)right[i];

        if (a < b)
            return -1;
        if (a > b)
            return 1;
        if (a == 0u)
            return 0;
    }

    return 0;
}

char *ef2_strcpy(
    char *destination,
    const char *source)
{
    ef2_size_t i = 0;

    do {
        destination[i] = source[i];
    } while (source[i++] != '\0');

    return destination;
}

char *ef2_strncpy(
    char *destination,
    const char *source,
    ef2_size_t size)
{
    ef2_size_t i = 0;

    while (i < size &&
           source[i] != '\0') {
        destination[i] = source[i];
        ++i;
    }

    while (i < size) {
        destination[i] = '\0';
        ++i;
    }

    return destination;
}

#ifndef EF2_LIBC_NO_STANDARD_ALIASES

void *memcpy(
    void *destination,
    const void *source,
    ef2_size_t size)
{
    return ef2_memcpy(
        destination,
        source,
        size);
}

void *memmove(
    void *destination,
    const void *source,
    ef2_size_t size)
{
    return ef2_memmove(
        destination,
        source,
        size);
}

void *memset(
    void *destination,
    int value,
    ef2_size_t size)
{
    return ef2_memset(
        destination,
        value,
        size);
}

int memcmp(
    const void *left,
    const void *right,
    ef2_size_t size)
{
    return ef2_memcmp(
        left,
        right,
        size);
}

ef2_size_t strlen(const char *text)
{
    return ef2_strlen(text);
}

int strcmp(
    const char *left,
    const char *right)
{
    return ef2_strcmp(left, right);
}

int strncmp(
    const char *left,
    const char *right,
    ef2_size_t size)
{
    return ef2_strncmp(
        left,
        right,
        size);
}

char *strcpy(
    char *destination,
    const char *source)
{
    return ef2_strcpy(
        destination,
        source);
}

char *strncpy(
    char *destination,
    const char *source,
    ef2_size_t size)
{
    return ef2_strncpy(
        destination,
        source,
        size);
}

#endif
