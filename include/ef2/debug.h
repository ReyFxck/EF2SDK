#ifndef EF2_DEBUG_H
#define EF2_DEBUG_H

#include <ef2/libc.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EF2_DEBUG_SIO_DEFAULT_BAUD 115200u

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
