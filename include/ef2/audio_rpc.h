#ifndef EF2_AUDIO_RPC_H
#define EF2_AUDIO_RPC_H

#include <ef2/base.h>

#define EF2_AUDIO_RPC_SID 0xEF2A0001u
#define EF2_AUDIO_RPC_MAX_FRAMES 512u

enum {
    EF2_AUDIO_RPC_INIT = 0,
    EF2_AUDIO_RPC_SUBMIT = 1,
    EF2_AUDIO_RPC_START = 2,
    EF2_AUDIO_RPC_STATS = 3,
    EF2_AUDIO_RPC_SET_VOLUME = 4,
    EF2_AUDIO_RPC_PAUSE = 5,
    EF2_AUDIO_RPC_RESUME = 6,
    EF2_AUDIO_RPC_STOP = 7,
    EF2_AUDIO_RPC_FLUSH = 8,
    EF2_AUDIO_RPC_SET_LATENCY = 9
};

#define EF2_AUDIO_RPC_FLAG_STARTED 0x01u
#define EF2_AUDIO_RPC_FLAG_PAUSED  0x02u

typedef struct {
    ef2_u32 frames;
    ef2_s16 samples[EF2_AUDIO_RPC_MAX_FRAMES * 2u];
} ef2_audio_rpc_submit;

typedef struct {
    ef2_u32 value;
} ef2_audio_rpc_control;

typedef struct {
    ef2_s32 result;
    ef2_u32 queued_frames;
    ef2_u32 capacity_frames;
    ef2_u32 underruns;
    ef2_u32 overruns;
    ef2_u32 latency_ms;
    ef2_u32 volume;
    ef2_u32 flags;
} ef2_audio_rpc_reply;

#endif
