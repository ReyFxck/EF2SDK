#ifndef EF2AUDIO_SPU2_DIRECT_H
#define EF2AUDIO_SPU2_DIRECT_H

typedef int (*ef2_spu2_transfer_callback)(void *arg);

int ef2_spu2_init(
    ef2_spu2_transfer_callback callback,
    void *callback_arg);

void ef2_spu2_set_volume(unsigned int volume);

int ef2_spu2_start_loop(
    void *buffer,
    unsigned int total_bytes);

int ef2_spu2_stop(void);

unsigned int ef2_spu2_active_block(void);

#endif
