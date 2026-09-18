#ifndef EF2_STORAGE_SIO2_H
#define EF2_STORAGE_SIO2_H
#include <ef2/base.h>
int ef2_storage_sio2_init(void);
int ef2_storage_sio2_exchange(
    ef2_u32 port,
    ef2_u32 ctrl1,
    ef2_u32 ctrl2,
    ef2_u32 baud_select,
    const ef2_u8 *tx,
    ef2_u32 tx_size,
    ef2_u8 *rx,
    ef2_u32 rx_size,
    ef2_u32 timeout_polls);
#endif
