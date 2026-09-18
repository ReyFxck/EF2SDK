#include <ef2/libc.h>

#include <stdarg.h>

typedef struct {
    char *buffer;
    ef2_size_t size;
    ef2_size_t count;
} ef2_format_output;

enum ef2_format_length {
    EF2_FORMAT_DEFAULT = 0,
    EF2_FORMAT_LONG,
    EF2_FORMAT_LONG_LONG,
    EF2_FORMAT_SIZE
};

static void format_putc(
    ef2_format_output *output,
    char value)
{
    if (output->buffer != (void *)0 &&
        output->size != 0u &&
        output->count + 1u < output->size) {
        output->buffer[output->count] = value;
    }

    ++output->count;
}

static void format_repeat(
    ef2_format_output *output,
    char value,
    ef2_u32 count)
{
    while (count != 0u) {
        format_putc(output, value);
        --count;
    }
}

static ef2_u32 format_unsigned_digits(
    unsigned long long value,
    ef2_u32 base,
    int upper,
    char *digits)
{
    static const unsigned long long decimal_places[] = {
        10000000000000000000ull,
        1000000000000000000ull,
        100000000000000000ull,
        10000000000000000ull,
        1000000000000000ull,
        100000000000000ull,
        10000000000000ull,
        1000000000000ull,
        100000000000ull,
        10000000000ull,
        1000000000ull,
        100000000ull,
        10000000ull,
        1000000ull,
        100000ull,
        10000ull,
        1000ull,
        100ull,
        10ull,
        1ull
    };
    static const char lower_table[] = "0123456789abcdef";
    static const char upper_table[] = "0123456789ABCDEF";
    const char *table = upper ? upper_table : lower_table;
    ef2_u32 count = 0;

    if (base == 16u) {
        int shift;
        int started = 0;

        for (shift = 60; shift >= 0; shift -= 4) {
            ef2_u32 digit =
                (ef2_u32)((value >> shift) & 0x0Full);

            if (digit != 0u || started || shift == 0) {
                digits[count++] = table[digit];
                started = 1;
            }
        }

        return count;
    }

    {
        ef2_u32 place_index;
        int started = 0;

        for (place_index = 0;
             place_index <
                 (ef2_u32)(sizeof(decimal_places) /
                           sizeof(decimal_places[0]));
             ++place_index) {
            unsigned long long place =
                decimal_places[place_index];
            ef2_u32 digit = 0;

            while (value >= place) {
                value -= place;
                ++digit;
            }

            if (digit != 0u || started ||
                place_index + 1u ==
                    (ef2_u32)(sizeof(decimal_places) /
                              sizeof(decimal_places[0]))) {
                digits[count++] = table[digit];
                started = 1;
            }
        }
    }

    return count;
}

static void format_unsigned(
    ef2_format_output *output,
    unsigned long long value,
    ef2_u32 base,
    int upper,
    ef2_u32 width,
    char pad,
    const char *prefix,
    ef2_u32 prefix_size)
{
    char digits[32];
    ef2_u32 digit_count =
        format_unsigned_digits(
            value,
            base,
            upper,
            digits);
    ef2_u32 field_size = digit_count + prefix_size;
    ef2_u32 padding =
        width > field_size
            ? width - field_size
            : 0u;
    ef2_u32 i;

    if (pad == '0') {
        for (i = 0; i < prefix_size; ++i)
            format_putc(output, prefix[i]);
        format_repeat(output, '0', padding);
    } else {
        format_repeat(output, ' ', padding);
        for (i = 0; i < prefix_size; ++i)
            format_putc(output, prefix[i]);
    }

    for (i = 0; i < digit_count; ++i)
        format_putc(output, digits[i]);
}

static unsigned long long format_unsigned_arg(
    va_list *args,
    enum ef2_format_length length)
{
    if (length == EF2_FORMAT_LONG_LONG)
        return va_arg(*args, unsigned long long);
    if (length == EF2_FORMAT_LONG)
        return va_arg(*args, unsigned long);
    if (length == EF2_FORMAT_SIZE)
        return va_arg(*args, ef2_size_t);

    return va_arg(*args, unsigned int);
}

static long long format_signed_arg(
    va_list *args,
    enum ef2_format_length length)
{
    if (length == EF2_FORMAT_LONG_LONG)
        return va_arg(*args, long long);
    if (length == EF2_FORMAT_LONG)
        return va_arg(*args, long);
    if (length == EF2_FORMAT_SIZE)
        return va_arg(*args, __PTRDIFF_TYPE__);

    return va_arg(*args, int);
}

static void format_signed(
    ef2_format_output *output,
    long long value,
    ef2_u32 width,
    char pad)
{
    unsigned long long magnitude;
    const char *prefix = "";
    ef2_u32 prefix_size = 0;

    if (value < 0) {
        magnitude =
            (unsigned long long)(-(value + 1));
        ++magnitude;
        prefix = "-";
        prefix_size = 1;
    } else {
        magnitude = (unsigned long long)value;
    }

    format_unsigned(
        output,
        magnitude,
        10,
        0,
        width,
        pad,
        prefix,
        prefix_size);
}

static void format_string(
    ef2_format_output *output,
    const char *text,
    ef2_u32 width)
{
    ef2_u32 length = 0;
    ef2_u32 i;

    if (text == (void *)0)
        text = "(null)";

    while (text[length] != '\0')
        ++length;

    if (width > length)
        format_repeat(output, ' ', width - length);

    for (i = 0; i < length; ++i)
        format_putc(output, text[i]);
}

int ef2_vsnprintf(
    char *buffer,
    ef2_size_t size,
    const char *format,
    va_list args)
{
    ef2_format_output output;
    va_list current;

    output.buffer = buffer;
    output.size = size;
    output.count = 0;

    va_copy(current, args);

    while (*format != '\0') {
        enum ef2_format_length length;
        ef2_u32 width;
        char pad;
        char specifier;

        if (*format != '%') {
            format_putc(&output, *format++);
            continue;
        }

        ++format;

        if (*format == '%') {
            format_putc(&output, '%');
            ++format;
            continue;
        }

        pad = ' ';
        width = 0;
        length = EF2_FORMAT_DEFAULT;

        if (*format == '0') {
            pad = '0';
            ++format;
        }

        while (*format >= '0' &&
               *format <= '9') {
            if (width < 100000u) {
                width =
                    (width << 3) +
                    (width << 1) +
                    (ef2_u32)(*format - '0');
            }
            ++format;
        }

        if (*format == 'l') {
            ++format;
            length = EF2_FORMAT_LONG;
            if (*format == 'l') {
                ++format;
                length = EF2_FORMAT_LONG_LONG;
            }
        } else if (*format == 'z') {
            ++format;
            length = EF2_FORMAT_SIZE;
        }

        specifier = *format;
        if (specifier == '\0') {
            format_putc(&output, '%');
            break;
        }
        ++format;

        switch (specifier) {
        case 'c':
            if (width > 1u)
                format_repeat(&output, ' ', width - 1u);
            format_putc(
                &output,
                (char)va_arg(current, int));
            break;

        case 's':
            format_string(
                &output,
                va_arg(current, const char *),
                width);
            break;

        case 'd':
        case 'i':
            format_signed(
                &output,
                format_signed_arg(
                    &current,
                    length),
                width,
                pad);
            break;

        case 'u':
            format_unsigned(
                &output,
                format_unsigned_arg(
                    &current,
                    length),
                10,
                0,
                width,
                pad,
                "",
                0);
            break;

        case 'x':
        case 'X':
            format_unsigned(
                &output,
                format_unsigned_arg(
                    &current,
                    length),
                16,
                specifier == 'X',
                width,
                pad,
                "",
                0);
            break;

        case 'p':
            format_unsigned(
                &output,
                (unsigned long long)
                    (__UINTPTR_TYPE__)
                    va_arg(current, void *),
                16,
                0,
                width,
                pad,
                "0x",
                2);
            break;

        default:
            format_putc(&output, '%');
            format_putc(&output, specifier);
            break;
        }
    }

    va_end(current);

    if (output.buffer != (void *)0 &&
        output.size != 0u) {
        ef2_size_t terminator =
            output.count < output.size
                ? output.count
                : output.size - 1u;

        output.buffer[terminator] = '\0';
    }

    if (output.count > (ef2_size_t)0x7FFFFFFFu)
        return 0x7FFFFFFF;

    return (int)output.count;
}

int ef2_snprintf(
    char *buffer,
    ef2_size_t size,
    const char *format,
    ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = ef2_vsnprintf(
        buffer,
        size,
        format,
        args);
    va_end(args);

    return result;
}

#ifndef EF2_LIBC_NO_STANDARD_ALIASES

int vsnprintf(
    char *buffer,
    ef2_size_t size,
    const char *format,
    va_list args)
{
    return ef2_vsnprintf(
        buffer,
        size,
        format,
        args);
}

int snprintf(
    char *buffer,
    ef2_size_t size,
    const char *format,
    ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = ef2_vsnprintf(
        buffer,
        size,
        format,
        args);
    va_end(args);

    return result;
}

#endif
