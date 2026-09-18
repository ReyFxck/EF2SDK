#ifndef EF2_PAD_RPC_H
#define EF2_PAD_RPC_H

#include <ef2/base.h>

#define EF2_PAD_RPC_SID 0xEF2B0001u
#define EF2_PAD_RPC_PORTS 2u
#define EF2_PAD_RPC_SLOTS 4u

enum {
    EF2_PAD_RPC_INIT = 0,
    EF2_PAD_RPC_POLL = 1,
    EF2_PAD_RPC_SET_RUMBLE = 2,
    EF2_PAD_RPC_POLL_SLOT = 3,
    EF2_PAD_RPC_REFRESH_TOPOLOGY = 4,
    EF2_PAD_RPC_SET_RUMBLE_SLOT = 5
};

typedef struct {
    ef2_u32 port_mask;
    ef2_u8 port;
    ef2_u8 slot;
    ef2_u8 small_motor;
    ef2_u8 large_motor;
    ef2_u8 reserved[4];
} ef2_pad_rpc_request;

typedef struct {
    ef2_u32 frame;
    ef2_u32 buttons;
    ef2_u32 errors;
    ef2_u32 reconnects;

    ef2_u8 connected;
    ef2_u8 raw_id;
    ef2_u8 timing_profile;
    ef2_u8 rumble_supported;
    ef2_u8 rumble_small;
    ef2_u8 rumble_large;

    ef2_u8 right_x;
    ef2_u8 right_y;
    ef2_u8 left_x;
    ef2_u8 left_y;

    ef2_u8 pressure[12];
    ef2_u8 reserved1[4];
} ef2_pad_rpc_port_state;

typedef struct {
    ef2_s32 result;
    ef2_u8 slot_count[EF2_PAD_RPC_PORTS];
    ef2_u8 reserved[2];

    /* Native slot zero for compatibility with ef2_pad_poll_all(). */
    ef2_pad_rpc_port_state port[EF2_PAD_RPC_PORTS];

    /* Result of EF2_PAD_RPC_POLL_SLOT / SET_RUMBLE_SLOT. */
    ef2_pad_rpc_port_state slot_state;
} ef2_pad_rpc_reply;

#endif
