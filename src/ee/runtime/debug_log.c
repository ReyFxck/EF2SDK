#include <ef2/debug.h>
#include <ef2/libc.h>
#include <ef2/stdio.h>

#include <stdarg.h>

#define EF2_DEBUG_FORMAT_BYTES 512u

static char g_debug_ring[EF2_DEBUG_RING_BYTES];
static ef2_u32 g_ring_head;
static ef2_u32 g_ring_used;
static ef2_u32 g_total_bytes;
static ef2_u32 g_sio_active;
static ef2_u32 g_sio_failures;

static void ring_write(
    const char *data,
    ef2_size_t size)
{
    ef2_size_t i;

    for (i = 0; i < size; ++i) {
        g_debug_ring[g_ring_head] = data[i];

        g_ring_head =
            (g_ring_head + 1u) %
            EF2_DEBUG_RING_BYTES;

        if (g_ring_used < EF2_DEBUG_RING_BYTES)
            ++g_ring_used;

        if (g_total_bytes != 0xFFFFFFFFu)
            ++g_total_bytes;
    }
}

ef2_s32 ef2_debug_write_raw(
    const char *data,
    ef2_size_t size)
{
    ef2_s32 serial_result;

    if (data == (const char *)0)
        return -1;

    ring_write(data, size);

    if (g_sio_active) {
        serial_result =
            ef2_debug_sio_write(
                (void *)0,
                data,
                size);

        if (serial_result < 0) {
            g_sio_active = 0;

            if (g_sio_failures != 0xFFFFFFFFu)
                ++g_sio_failures;
        }
    }

    if (size > (ef2_size_t)0x7FFFFFFFu)
        return 0x7FFFFFFF;

    return (ef2_s32)size;
}

static ef2_s32 debug_sink_write(
    void *context,
    const char *data,
    ef2_size_t size)
{
    (void)context;

    /*
     * The RAM sink succeeds independently of the optional serial mirror.
     * Logging must never turn a missing serial interface into an app error.
     */
    return ef2_debug_write_raw(data, size);
}

void ef2_debug_reset(void)
{
    g_ring_head = 0;
    g_ring_used = 0;
    g_total_bytes = 0;
    g_sio_active = 0;
    g_sio_failures = 0;

    ef2_memset(
        g_debug_ring,
        0,
        sizeof(g_debug_ring));
}

int ef2_debug_init(ef2_u32 baudrate)
{
    int sio_result;

    ef2_debug_reset();

    if (baudrate == 0u)
        baudrate =
            EF2_DEBUG_SIO_DEFAULT_BAUD;

    sio_result =
        ef2_debug_sio_init(baudrate);

    if (sio_result == 0)
        g_sio_active = 1;
    else
        g_sio_failures = 1;

    ef2_stdio_set_stdout(
        debug_sink_write,
        (void *)0);

    ef2_stdio_set_stderr(
        debug_sink_write,
        (void *)0);

    return 0;
}

int ef2_debug_printf(
    const char *tag,
    const char *format,
    ...)
{
    char buffer[EF2_DEBUG_FORMAT_BYTES];
    va_list args;
    int length;
    int result = 0;

    if (format == (const char *)0)
        return -1;

    if (tag != (const char *)0 &&
        tag[0] != '\0') {
        if (ef2_printf(
                "[EF2][%s] ",
                tag) < 0)
            result = -1;
    } else {
        if (ef2_printf("[EF2] ") < 0)
            result = -1;
    }

    va_start(args, format);
    length = ef2_vsnprintf(
        buffer,
        sizeof(buffer),
        format,
        args);
    va_end(args);

    if (length < 0)
        return -1;

    if ((ef2_size_t)length >=
        sizeof(buffer)) {
        static const char truncated[] =
            "<debug message truncated>\n";

        if (ef2_fprintf(
                &ef2_stdout_stream,
                "%s",
                truncated) < 0)
            return -1;

        return result;
    }

    if (ef2_fprintf(
            &ef2_stdout_stream,
            "%s",
            buffer) < 0)
        result = -1;

    return result;
}

ef2_size_t ef2_debug_copy_recent(
    char *destination,
    ef2_size_t capacity)
{
    ef2_u32 start;
    ef2_u32 count;
    ef2_u32 i;

    if (destination == (char *)0 ||
        capacity == 0u)
        return 0;

    count = g_ring_used;

    if ((ef2_size_t)count >= capacity)
        count = (ef2_u32)capacity - 1u;

    start =
        (g_ring_head +
         EF2_DEBUG_RING_BYTES -
         g_ring_used) %
        EF2_DEBUG_RING_BYTES;

    if (count < g_ring_used) {
        start =
            (g_ring_head +
             EF2_DEBUG_RING_BYTES -
             count) %
            EF2_DEBUG_RING_BYTES;
    }

    for (i = 0; i < count; ++i) {
        destination[i] =
            g_debug_ring[
                (start + i) %
                EF2_DEBUG_RING_BYTES];
    }

    destination[count] = '\0';
    return (ef2_size_t)count;
}

int ef2_debug_get_stats(
    ef2_debug_stats *stats)
{
    if (stats == (ef2_debug_stats *)0)
        return -1;

    stats->ring_capacity =
        EF2_DEBUG_RING_BYTES;
    stats->ring_used = g_ring_used;
    stats->total_bytes = g_total_bytes;
    stats->sio_active = g_sio_active;
    stats->sio_failures = g_sio_failures;

    return 0;
}
