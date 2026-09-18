#ifndef EF2_AUDIO_RPC_H
#define EF2_AUDIO_RPC_H

#include <ef2/base.h>

#define EF2_AUDIO_RPC_SID 0xEF2A0001u
#define EF2_AUDIO_RPC_MAX_FRAMES 512u

enum {
    EF2_AUDIO_RPC_INIT = 0,
    EF2_AUDIO_RPC_SUBMIT = 1,
    EF2_AUDIO_RPC_START = 2,
    EF2_AUDIO_RPC_STATS = 3
};

typedef struct {
    ef2_u32 frames;
    ef2_s16 samples[EF2_AUDIO_RPC_MAX_FRAMES * 2u];
} ef2_audio_rpc_submit;

typedef struct {
    ef2_s32 result;
    ef2_u32 queued_frames;
    ef2_u32 capacity_frames;
    ef2_u32 underruns;
    ef2_u32 overruns;
} ef2_audio_rpc_reply;

#endif
