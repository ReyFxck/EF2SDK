#include "irx_imports.h"
#include <ef2/audio_rpc.h>

#define EF2AUDIO_RING_FRAMES 8192
#define EF2AUDIO_BLOCK_FRAMES 512
#define EF2AUDIO_BLOCK_DMA_CHANNEL 1
#define EF2AUDIO_MAX_VOLUME 0x3FFF
#define EF2AUDIO_SD_CORE_0 0
#define EF2AUDIO_SD_CORE_1 1
#define EF2AUDIO_SD_INIT_COLD 0

IRX_ID("ef2audio", 1, 0);

static SifRpcDataQueue_t g_rpc_queue;
static SifRpcServerData_t g_rpc_server;
static unsigned char g_rpc_input[4096] __attribute__((aligned(64)));
static ef2_audio_rpc_reply g_rpc_reply __attribute__((aligned(64)));

static ef2_s16 g_ring[EF2AUDIO_RING_FRAMES * 2] __attribute__((aligned(64)));
static ef2_u32 g_read_frame;
static ef2_u32 g_write_frame;
static ef2_u32 g_queued_frames;
static ef2_u32 g_underruns;
static ef2_u32 g_overruns;

static unsigned char g_spu_buffer[4096] __attribute__((aligned(64)));

static int g_ring_mutex = -1;
static int g_space_sema = -1;
static int g_transfer_sema = -1;
static int g_rpc_ready_sema = -1;
static int g_play_thread = -1;
static int g_initialized;
static int g_started;

static void clear_bytes(void *ptr, ef2_u32 size)
{
    unsigned char *p = (unsigned char *)ptr;
    ef2_u32 i;
    for (i = 0; i < size; ++i)
        p[i] = 0;
}

static int create_semaphore(int initial, int max)
{
    iop_sema_t sema;
    sema.attr = 0;
    sema.option = 0;
    sema.initial = initial;
    sema.max = max;
    return CreateSema(&sema);
}

static void update_volume(int audible)
{
    int volume = audible ? EF2AUDIO_MAX_VOLUME : 0;

    sceSdSetParam(EF2AUDIO_SD_CORE_1 | SD_PARAM_AVOLL, 0x7FFF);
    sceSdSetParam(EF2AUDIO_SD_CORE_1 | SD_PARAM_AVOLR, 0x7FFF);
    sceSdSetParam(EF2AUDIO_SD_CORE_0 | SD_PARAM_BVOLL, 0);
    sceSdSetParam(EF2AUDIO_SD_CORE_0 | SD_PARAM_BVOLR, 0);
    sceSdSetParam(EF2AUDIO_SD_CORE_1 | SD_PARAM_BVOLL, volume);
    sceSdSetParam(EF2AUDIO_SD_CORE_1 | SD_PARAM_BVOLR, volume);
    sceSdSetParam(EF2AUDIO_SD_CORE_0 | SD_PARAM_MVOLL, 0);
    sceSdSetParam(EF2AUDIO_SD_CORE_0 | SD_PARAM_MVOLR, 0);
    sceSdSetParam(EF2AUDIO_SD_CORE_1 | SD_PARAM_MVOLL, EF2AUDIO_MAX_VOLUME);
    sceSdSetParam(EF2AUDIO_SD_CORE_1 | SD_PARAM_MVOLR, EF2AUDIO_MAX_VOLUME);
}

static int transfer_complete(void *arg)
{
    (void)arg;
    if (g_transfer_sema >= 0)
        iSignalSema(g_transfer_sema);
    return 1;
}

static void write_spu_frame(
    unsigned char *block,
    ef2_u32 index,
    ef2_s16 left,
    ef2_s16 right)
{
    ef2_s16 *left_half;
    ef2_s16 *right_half;

    if (index < 256u) {
        left_half = (ef2_s16 *)(block + 0);
        right_half = (ef2_s16 *)(block + 512);
        left_half[index] = left;
        right_half[index] = right;
    } else {
        left_half = (ef2_s16 *)(block + 1024);
        right_half = (ef2_s16 *)(block + 1536);
        left_half[index - 256u] = left;
        right_half[index - 256u] = right;
    }
}

static void fill_spu_block(unsigned char *block)
{
    ef2_u32 take;
    ef2_u32 i;

    WaitSema(g_ring_mutex);

    take = g_queued_frames;
    if (take > EF2AUDIO_BLOCK_FRAMES)
        take = EF2AUDIO_BLOCK_FRAMES;

    for (i = 0; i < EF2AUDIO_BLOCK_FRAMES; ++i) {
        ef2_s16 left = 0;
        ef2_s16 right = 0;

        if (i < take) {
            ef2_u32 position =
                ((g_read_frame + i) % EF2AUDIO_RING_FRAMES) * 2u;
            left = g_ring[position];
            right = g_ring[position + 1u];
        }

        write_spu_frame(block, i, left, right);
    }

    if (take != 0) {
        g_read_frame = (g_read_frame + take) % EF2AUDIO_RING_FRAMES;
        g_queued_frames -= take;
    }

    if (take < EF2AUDIO_BLOCK_FRAMES)
        ++g_underruns;

    SignalSema(g_ring_mutex);

    if (take != 0)
        SignalSema(g_space_sema);

    FlushDcache();
}

static void play_thread(void *arg)
{
    (void)arg;

    for (;;) {
        ef2_u32 status;
        ef2_u32 active_block;
        ef2_u32 idle_block;

        WaitSema(g_transfer_sema);

        status = sceSdBlockTransStatus(EF2AUDIO_BLOCK_DMA_CHANNEL, 0);
        active_block = (status >> 24) & 1u;
        idle_block = 1u - active_block;

        fill_spu_block(g_spu_buffer + (idle_block << 11));
    }
}

static int audio_initialize(void)
{
    iop_thread_t thread;

    if (g_initialized)
        return 0;

    if (sceSdInit(EF2AUDIO_SD_INIT_COLD) < 0)
        return -1;

    g_ring_mutex = create_semaphore(1, 1);
    g_space_sema = create_semaphore(0, 1);
    g_transfer_sema = create_semaphore(0, 1);

    if (g_ring_mutex < 0 || g_space_sema < 0 || g_transfer_sema < 0)
        return -2;

    clear_bytes(g_ring, sizeof(g_ring));
    clear_bytes(g_spu_buffer, sizeof(g_spu_buffer));
    g_read_frame = 0;
    g_write_frame = 0;
    g_queued_frames = 0;
    g_underruns = 0;
    g_overruns = 0;

    update_volume(0);
    sceSdSetTransCallback(EF2AUDIO_BLOCK_DMA_CHANNEL, (void *)transfer_complete);

    thread.attr = TH_C;
    thread.option = 0;
    thread.thread = play_thread;
    thread.stacksize = 0x1000;
    thread.priority = 38;

    g_play_thread = CreateThread(&thread);
    if (g_play_thread < 0)
        return -3;

    if (StartThread(g_play_thread, 0) < 0)
        return -4;

    g_initialized = 1;
    return 0;
}

static int audio_submit(const ef2_s16 *samples, ef2_u32 frames)
{
    ef2_u32 source_frame = 0;

    if (!g_initialized)
        return -1;
    if (frames > EF2_AUDIO_RPC_MAX_FRAMES)
        return -2;

    while (source_frame < frames) {
        ef2_u32 space;
        ef2_u32 count;
        ef2_u32 i;

        WaitSema(g_ring_mutex);
        space = EF2AUDIO_RING_FRAMES - g_queued_frames;

        if (space == 0) {
            SignalSema(g_ring_mutex);
            ++g_overruns;
            WaitSema(g_space_sema);
            continue;
        }

        count = frames - source_frame;
        if (count > space)
            count = space;

        for (i = 0; i < count; ++i) {
            ef2_u32 position =
                ((g_write_frame + i) % EF2AUDIO_RING_FRAMES) * 2u;
            ef2_u32 source = (source_frame + i) * 2u;

            g_ring[position] = samples[source];
            g_ring[position + 1u] = samples[source + 1u];
        }

        g_write_frame = (g_write_frame + count) % EF2AUDIO_RING_FRAMES;
        g_queued_frames += count;
        source_frame += count;
        SignalSema(g_ring_mutex);
    }

    return (int)frames;
}

static int audio_start(void)
{
    if (!g_initialized)
        return -1;
    if (g_started)
        return 0;

    clear_bytes(g_spu_buffer, sizeof(g_spu_buffer));
    FlushDcache();
    update_volume(1);

    if (sceSdBlockTrans(
            EF2AUDIO_BLOCK_DMA_CHANNEL,
            SD_TRANS_LOOP,
            g_spu_buffer,
            sizeof(g_spu_buffer),
            0) < 0) {
        update_volume(0);
        return -2;
    }

    g_started = 1;
    return 0;
}

static void fill_reply(int result)
{
    g_rpc_reply.result = result;
    g_rpc_reply.queued_frames = g_queued_frames;
    g_rpc_reply.capacity_frames = EF2AUDIO_RING_FRAMES;
    g_rpc_reply.underruns = g_underruns;
    g_rpc_reply.overruns = g_overruns;
}

static void *rpc_handler(int function, void *buffer, int length)
{
    int result = 0;

    switch (function) {
        case EF2_AUDIO_RPC_INIT:
            result = audio_initialize();
            break;

        case EF2_AUDIO_RPC_SUBMIT:
        {
            ef2_audio_rpc_submit *submit = (ef2_audio_rpc_submit *)buffer;
            ef2_u32 expected;

            if (length < (int)sizeof(ef2_u32)) {
                result = -10;
                break;
            }

            expected =
                sizeof(ef2_u32) +
                submit->frames * 2u * sizeof(ef2_s16);

            if (submit->frames > EF2_AUDIO_RPC_MAX_FRAMES ||
                (ef2_u32)length < expected) {
                result = -11;
                break;
            }

            result = audio_submit(submit->samples, submit->frames);
            break;
        }

        case EF2_AUDIO_RPC_START:
            result = audio_start();
            break;

        case EF2_AUDIO_RPC_STATS:
            result = 0;
            break;

        default:
            result = -100;
            break;
    }

    fill_reply(result);
    return &g_rpc_reply;
}

static void rpc_thread(void *arg)
{
    (void)arg;

    /*
     * Match the long-standing PS2 IOP RPC pattern: initialize, attach the
     * queue to the thread that will actually service it, register the SID,
     * then enter the loop from the same thread context.
     */
    sceSifInitRpc(0);
    sceSifSetRpcQueue(&g_rpc_queue, GetThreadId());
    sceSifRegisterRpc(
        &g_rpc_server,
        EF2_AUDIO_RPC_SID,
        rpc_handler,
        g_rpc_input,
        0,
        0,
        &g_rpc_queue);

    SignalSema(g_rpc_ready_sema);
    sceSifRpcLoop(&g_rpc_queue);
}

int _start(int argc, char *argv[])
{
    iop_thread_t thread;
    int thread_id;

    (void)argc;
    (void)argv;

    FlushDcache();
    CpuEnableIntr();

    thread.attr = TH_C;
    thread.option = 0;
    thread.thread = rpc_thread;
    thread.stacksize = 0x1000;
    thread.priority = 40;

    /*
     * Registering an IOP SIFRPC queue from _start is subtly different from
     * the pattern used by established PS2 modules. Run the complete RPC setup
     * in the worker thread itself, but make module start wait until that
     * thread explicitly confirms registration. This keeps the standard thread
     * context without reintroducing the EE/IOP bind race.
     */
    g_rpc_ready_sema = create_semaphore(0, 1);
    if (g_rpc_ready_sema < 0)
        return MODULE_NO_RESIDENT_END;

    thread_id = CreateThread(&thread);
    if (thread_id < 0)
        return MODULE_NO_RESIDENT_END;

    if (StartThread(thread_id, 0) < 0)
        return MODULE_NO_RESIDENT_END;

    WaitSema(g_rpc_ready_sema);

    return MODULE_RESIDENT_END;
}
