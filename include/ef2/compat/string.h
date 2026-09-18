#ifndef EF2_COMPAT_STRING_H
#define EF2_COMPAT_STRING_H

#include <ef2/libc.h>

#ifndef __cplusplus
#define EF2_RESTRICT restrict
#else
#define EF2_RESTRICT
#endif

typedef ef2_size_t size_t;

void *memcpy(
    void *EF2_RESTRICT destination,
    const void *EF2_RESTRICT source,
    size_t size);

void *memmove(
    void *destination,
    const void *source,
    size_t size);

void *memset(
    void *destination,
    int value,
    size_t size);

int memcmp(
    const void *left,
    const void *right,
    size_t size);

size_t strlen(const char *text);

int strcmp(
    const char *left,
    const char *right);

int strncmp(
    const char *left,
    const char *right,
    size_t size);

char *strcpy(
    char *EF2_RESTRICT destination,
    const char *EF2_RESTRICT source);

char *strncpy(
    char *EF2_RESTRICT destination,
    const char *EF2_RESTRICT source,
    size_t size);

#undef EF2_RESTRICT

#endif
