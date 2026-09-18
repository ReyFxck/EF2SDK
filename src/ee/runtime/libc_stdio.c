#include <ef2/heap.h>
#include <ef2/libc.h>
#include <ef2/stdio.h>

#include <stdarg.h>

#define EF2_STDIO_STACK_BUFFER 256u

ef2_FILE ef2_stdout_stream = {
    (ef2_stdio_write_fn)0,
    (void *)0
};

ef2_FILE ef2_stderr_stream = {
    (ef2_stdio_write_fn)0,
    (void *)0
};

static void set_stream(
    ef2_FILE *stream,
    ef2_stdio_write_fn write,
    void *context)
{
    stream->write = write;
    stream->context = context;
}

void ef2_stdio_set_stdout(
    ef2_stdio_write_fn write,
    void *context)
{
    set_stream(
        &ef2_stdout_stream,
        write,
        context);
}

void ef2_stdio_set_stderr(
    ef2_stdio_write_fn write,
    void *context)
{
    set_stream(
        &ef2_stderr_stream,
        write,
        context);
}

static int write_all(
    ef2_FILE *stream,
    const char *data,
    ef2_size_t size)
{
    ef2_size_t written = 0;

    if (stream == (ef2_FILE *)0 ||
        stream->write == (ef2_stdio_write_fn)0)
        return -1;

    while (written < size) {
        ef2_s32 result =
            stream->write(
                stream->context,
                data + written,
                size - written);

        if (result <= 0)
            return -1;

        if ((ef2_size_t)result >
            size - written)
            return -1;

        written += (ef2_size_t)result;
    }

    if (written > (ef2_size_t)0x7FFFFFFFu)
        return 0x7FFFFFFF;

    return (int)written;
}

int ef2_vfprintf(
    ef2_FILE *stream,
    const char *format,
    va_list args)
{
    char stack_buffer[EF2_STDIO_STACK_BUFFER];
    char *buffer = stack_buffer;
    ef2_size_t capacity;
    va_list measure_args;
    va_list format_args;
    int needed;
    int formatted;
    int result;

    if (stream == (ef2_FILE *)0 ||
        format == (const char *)0)
        return -1;

    va_copy(measure_args, args);
    needed = ef2_vsnprintf(
        (char *)0,
        0,
        format,
        measure_args);
    va_end(measure_args);

    if (needed < 0)
        return -1;

    capacity = (ef2_size_t)needed + 1u;

    if (capacity > sizeof(stack_buffer)) {
        buffer = (char *)ef2_malloc(capacity);
        if (buffer == (char *)0)
            return -1;
    }

    va_copy(format_args, args);
    formatted = ef2_vsnprintf(
        buffer,
        capacity,
        format,
        format_args);
    va_end(format_args);

    if (formatted < 0) {
        if (buffer != stack_buffer)
            ef2_free(buffer);
        return -1;
    }

    result = write_all(
        stream,
        buffer,
        (ef2_size_t)formatted);

    if (buffer != stack_buffer)
        ef2_free(buffer);

    if (result < 0)
        return -1;

    return formatted;
}

int ef2_fprintf(
    ef2_FILE *stream,
    const char *format,
    ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = ef2_vfprintf(
        stream,
        format,
        args);
    va_end(args);

    return result;
}

int ef2_vprintf(
    const char *format,
    va_list args)
{
    return ef2_vfprintf(
        &ef2_stdout_stream,
        format,
        args);
}

int ef2_printf(
    const char *format,
    ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = ef2_vprintf(
        format,
        args);
    va_end(args);

    return result;
}

int ef2_puts(const char *text)
{
    ef2_size_t length;

    if (text == (const char *)0)
        return -1;

    length = ef2_strlen(text);

    if (write_all(
            &ef2_stdout_stream,
            text,
            length) < 0)
        return -1;

    if (write_all(
            &ef2_stdout_stream,
            "\n",
            1) < 0)
        return -1;

    if (length >=
        (ef2_size_t)0x7FFFFFFFu)
        return 0x7FFFFFFF;

    return (int)length + 1;
}

#ifndef EF2_LIBC_NO_STANDARD_ALIASES

int vfprintf(
    ef2_FILE *stream,
    const char *format,
    va_list args)
{
    return ef2_vfprintf(
        stream,
        format,
        args);
}

int fprintf(
    ef2_FILE *stream,
    const char *format,
    ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = ef2_vfprintf(
        stream,
        format,
        args);
    va_end(args);

    return result;
}

int vprintf(
    const char *format,
    va_list args)
{
    return ef2_vprintf(format, args);
}

int printf(
    const char *format,
    ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = ef2_vprintf(
        format,
        args);
    va_end(args);

    return result;
}

int puts(const char *text)
{
    return ef2_puts(text);
}

#endif
