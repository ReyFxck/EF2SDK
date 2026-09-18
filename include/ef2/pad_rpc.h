#ifndef EF2_PAD_RPC_H
#define EF2_PAD_RPC_H

#include <ef2/base.h>

#define EF2_PAD_RPC_SID 0xEF2B0001u

enum {
    EF2_PAD_RPC_INIT = 0,
    EF2_PAD_RPC_POLL = 1
};

typedef struct {
    ef2_u32 port_mask;
} ef2_pad_rpc_request;

typedef struct {
    ef2_u32 frame;
    ef2_u32 buttons;
    ef2_u32 errors;
    ef2_u32 reconnects;

    ef2_u8 connected;
    ef2_u8 raw_id;
    ef2_u8 timing_profile;
    ef2_u8 reserved0;

    ef2_u8 right_x;
    ef2_u8 right_y;
    ef2_u8 left_x;
    ef2_u8 left_y;

    ef2_u8 pressure[12];
    ef2_u8 reserved1[4];
} ef2_pad_rpc_port_state;

typedef struct {
    ef2_s32 result;
    ef2_pad_rpc_port_state port[2];
} ef2_pad_rpc_reply;

#endif
