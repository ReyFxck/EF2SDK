#ifndef EF2_COMPAT_STDIO_H
#define EF2_COMPAT_STDIO_H

#include <stdarg.h>

#include <ef2/libc.h>

typedef ef2_size_t size_t;

int vsnprintf(
    char *buffer,
    size_t size,
    const char *format,
    va_list args);

int snprintf(
    char *buffer,
    size_t size,
    const char *format,
    ...);

#endif
