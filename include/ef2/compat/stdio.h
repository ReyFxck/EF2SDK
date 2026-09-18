#ifndef EF2_COMPAT_STDIO_H
#define EF2_COMPAT_STDIO_H

#include <stdarg.h>

#include <ef2/stdio.h>

typedef ef2_size_t size_t;
typedef ef2_FILE FILE;

#define stdout (&ef2_stdout_stream)
#define stderr (&ef2_stderr_stream)

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

int vfprintf(
    FILE *stream,
    const char *format,
    va_list args);

int fprintf(
    FILE *stream,
    const char *format,
    ...);

int vprintf(
    const char *format,
    va_list args);

int printf(
    const char *format,
    ...);

int puts(const char *text);

#endif
