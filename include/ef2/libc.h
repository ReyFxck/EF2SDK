#ifndef EF2_LIBC_H
#define EF2_LIBC_H

#include <ef2/base.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef __SIZE_TYPE__ ef2_size_t;

void *ef2_memcpy(
    void *destination,
    const void *source,
    ef2_size_t size);

void *ef2_memmove(
    void *destination,
    const void *source,
    ef2_size_t size);

void *ef2_memset(
    void *destination,
    int value,
    ef2_size_t size);

int ef2_memcmp(
    const void *left,
    const void *right,
    ef2_size_t size);

ef2_size_t ef2_strlen(const char *text);

int ef2_strcmp(
    const char *left,
    const char *right);

int ef2_strncmp(
    const char *left,
    const char *right,
    ef2_size_t size);

char *ef2_strcpy(
    char *destination,
    const char *source);

char *ef2_strncpy(
    char *destination,
    const char *source,
    ef2_size_t size);

#ifdef __cplusplus
}
#endif

#endif
