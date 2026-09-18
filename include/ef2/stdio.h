#ifndef EF2_STDIO_H
#define EF2_STDIO_H

#include <ef2/libc.h>

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef ef2_s32 (*ef2_stdio_write_fn)(
    void *context,
    const char *data,
    ef2_size_t size);

typedef struct {
    ef2_stdio_write_fn write;
    void *context;
} ef2_FILE;

extern ef2_FILE ef2_stdout_stream;
extern ef2_FILE ef2_stderr_stream;

void ef2_stdio_set_stdout(
    ef2_stdio_write_fn write,
    void *context);

void ef2_stdio_set_stderr(
    ef2_stdio_write_fn write,
    void *context);

int ef2_vfprintf(
    ef2_FILE *stream,
    const char *format,
    va_list args);

int ef2_fprintf(
    ef2_FILE *stream,
    const char *format,
    ...);

int ef2_vprintf(
    const char *format,
    va_list args);

int ef2_printf(
    const char *format,
    ...);

int ef2_puts(const char *text);

#ifdef __cplusplus
}
#endif

#endif
