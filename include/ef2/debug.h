#ifndef EF2_DEBUG_H
#define EF2_DEBUG_H

#include <ef2/libc.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EF2_DEBUG_SIO_DEFAULT_BAUD 115200u
#define EF2_DEBUG_RING_BYTES 8192u

typedef struct {
    ef2_u32 ring_capacity;
    ef2_u32 ring_used;
    ef2_u32 total_bytes;
    ef2_u32 sio_active;
    ef2_u32 sio_failures;
} ef2_debug_stats;

/*
 * Initializes the always-on RAM log and attempts to attach SIO as a mirror.
 * Failure to attach SIO is non-fatal: RAM logging remains active.
 */
int ef2_debug_init(ef2_u32 baudrate);

void ef2_debug_reset(void);

int ef2_debug_printf(
    const char *tag,
    const char *format,
    ...);

ef2_size_t ef2_debug_copy_recent(
    char *destination,
    ef2_size_t capacity);

int ef2_debug_get_stats(
    ef2_debug_stats *stats);

int ef2_debug_sio_init(ef2_u32 baudrate);

ef2_s32 ef2_debug_sio_write(
    void *context,
    const char *data,
    ef2_size_t size);

int ef2_debug_use_sio_stdio(ef2_u32 baudrate);

#ifdef __cplusplus
}
#endif

#endif
