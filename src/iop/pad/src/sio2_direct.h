#ifndef EF2PAD_SIO2_DIRECT_H
#define EF2PAD_SIO2_DIRECT_H

#include <types.h>

typedef struct {
    u32 recv1;
    u32 recv2;
    u32 recv3;
} ef2_sio2_result;

int ef2_sio2_init(void);

int ef2_sio2_transfer_pad(
    unsigned int port,
    unsigned int timing_profile,
    unsigned int stat70_bit,
    const u8 *input,
    unsigned int size,
    u8 *output,
    ef2_sio2_result *result);

#endif
