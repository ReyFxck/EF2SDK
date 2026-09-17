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

#define EF2_LOG_PREFIX "[EF2AUDIO] "

#define EF2_LOG(...)            \
    do {                        \
        printf(__VA_ARGS__);    \
        Kprintf(__VA_ARGS__);   \
    } while (0)

static SifRpcDataQueue_t g_rpc_queue;
static SifRpcServerData_t g_rpc_server;
static SifRpcServerData_t g_rpc_server_fallback;
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
    int result;

    sema.attr = 0;
    sema.option = 0;
    sema.initial = initial;
    sema.max = max;

    result = CreateSema(&sema);
    EF2_LOG(
        EF2_LOG_PREFIX "CreateSema initial=%d max=%d -> %d\n",
        initial,
        max,
        result);

    return result;
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
    int sd_init_result;

    EF2_LOG(EF2_LOG_PREFIX "audio_initialize entered initialized=%d\n", g_initialized);

    if (g_initialized)
        return 0;

    sd_init_result = sceSdInit(EF2AUDIO_SD_INIT_COLD);
    EF2_LOG(EF2_LOG_PREFIX "sceSdInit -> %d\n", sd_init_result);

    if (sd_init_result < 0)
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
    EF2_LOG(EF2_LOG_PREFIX "audio play CreateThread -> %d\n", g_play_thread);

    if (g_play_thread < 0)
        return -3;

    {
        int start_result = StartThread(g_play_thread, 0);
        EF2_LOG(EF2_LOG_PREFIX "audio play StartThread -> %d\n", start_result);

        if (start_result < 0)
            return -4;
    }

    g_initialized = 1;
    EF2_LOG(EF2_LOG_PREFIX "audio_initialize success\n");
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
    int transfer_result;

    EF2_LOG(
        EF2_LOG_PREFIX "audio_start initialized=%d started=%d queued=%u\n",
        g_initialized,
        g_started,
        (unsigned int)g_queued_frames);

    if (!g_initialized)
        return -1;
    if (g_started)
        return 0;

    clear_bytes(g_spu_buffer, sizeof(g_spu_buffer));
    FlushDcache();
    update_volume(1);

    transfer_result = sceSdBlockTrans(
        EF2AUDIO_BLOCK_DMA_CHANNEL,
        SD_TRANS_LOOP,
        g_spu_buffer,
        sizeof(g_spu_buffer),
        0);

    EF2_LOG(EF2_LOG_PREFIX "sceSdBlockTrans -> %d\n", transfer_result);

    if (transfer_result < 0) {
        update_volume(0);
        return -2;
    }

    g_started = 1;
    EF2_LOG(EF2_LOG_PREFIX "audio_start success\n");
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

    EF2_LOG(
        EF2_LOG_PREFIX "rpc_handler fn=%d length=%d queue=%u underruns=%u overruns=%u\n",
        function,
        length,
        (unsigned int)g_queued_frames,
        (unsigned int)g_underruns,
        (unsigned int)g_overruns);

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

    EF2_LOG(
        EF2_LOG_PREFIX "rpc_handler fn=%d result=%d queued=%u\n",
        function,
        result,
        (unsigned int)g_queued_frames);

    return &g_rpc_reply;
}

static void rpc_thread(void *arg)
{
    int tid;
    int signal_result;

    (void)arg;

    tid = GetThreadId();

    EF2_LOG(
        EF2_LOG_PREFIX "rpc_thread entered tid=%d queue=%08x server=%08x\n",
        tid,
        (unsigned int)&g_rpc_queue,
        (unsigned int)&g_rpc_server);

    EF2_LOG(EF2_LOG_PREFIX "calling sceSifInitRpc\n");
    sceSifInitRpc(0);
    EF2_LOG(EF2_LOG_PREFIX "sceSifInitRpc returned\n");

    EF2_LOG(EF2_LOG_PREFIX "calling sceSifSetRpcQueue tid=%d\n", tid);
    sceSifSetRpcQueue(&g_rpc_queue, tid);
    EF2_LOG(
        EF2_LOG_PREFIX "queue registered thread_id=%d link=%08x next=%08x active=%d\n",
        g_rpc_queue.thread_id,
        (unsigned int)g_rpc_queue.link,
        (unsigned int)g_rpc_queue.next,
        g_rpc_queue.active);
    EF2_LOG(
        EF2_LOG_PREFIX "register primary SID=%08x\n",
        (unsigned int)EF2_AUDIO_RPC_SID_PRIMARY);

    sceSifRegisterRpc(
        &g_rpc_server,
        EF2_AUDIO_RPC_SID_PRIMARY,
        rpc_handler,
        g_rpc_input,
        0,
        0,
        &g_rpc_queue);

    EF2_LOG(
        EF2_LOG_PREFIX "primary registered server.sid=%08x server.base=%08x queue.link=%08x\n",
        (unsigned int)g_rpc_server.sid,
        (unsigned int)g_rpc_server.base,
        (unsigned int)g_rpc_queue.link);

    EF2_LOG(
        EF2_LOG_PREFIX "register fallback SID=%08x\n",
        (unsigned int)EF2_AUDIO_RPC_SID_FALLBACK);

    sceSifRegisterRpc(
        &g_rpc_server_fallback,
        EF2_AUDIO_RPC_SID_FALLBACK,
        rpc_handler,
        g_rpc_input,
        0,
        0,
        &g_rpc_queue);

    EF2_LOG(
        EF2_LOG_PREFIX "fallback registered server.sid=%08x server.base=%08x primary.link=%08x\n",
        (unsigned int)g_rpc_server_fallback.sid,
        (unsigned int)g_rpc_server_fallback.base,
        (unsigned int)g_rpc_server.link);

    signal_result = SignalSema(g_rpc_ready_sema);
    EF2_LOG(
        EF2_LOG_PREFIX "rpc ready SignalSema(%d) -> %d\n",
        g_rpc_ready_sema,
        signal_result);

    EF2_LOG(EF2_LOG_PREFIX "entering sceSifRpcLoop\n");
    sceSifRpcLoop(&g_rpc_queue);

    EF2_LOG(EF2_LOG_PREFIX "ERROR: sceSifRpcLoop returned\n");
}

int _start(int argc, char *argv[])
{
    iop_thread_t thread;
    int thread_id;
    int start_result;
    int wait_result;

    (void)argv;

    EF2_LOG(
        EF2_LOG_PREFIX "_start entered argc=%d primary=%08x fallback=%08x\n",
        argc,
        (unsigned int)EF2_AUDIO_RPC_SID_PRIMARY,
        (unsigned int)EF2_AUDIO_RPC_SID_FALLBACK);

    FlushDcache();
    EF2_LOG(EF2_LOG_PREFIX "FlushDcache done\n");

    CpuEnableIntr();
    EF2_LOG(EF2_LOG_PREFIX "CpuEnableIntr done\n");

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
    EF2_LOG(
        EF2_LOG_PREFIX "rpc ready sema=%d\n",
        g_rpc_ready_sema);

    if (g_rpc_ready_sema < 0) {
        EF2_LOG(EF2_LOG_PREFIX "ERROR: rpc ready sema creation failed\n");
        return MODULE_NO_RESIDENT_END;
    }

    thread_id = CreateThread(&thread);
    EF2_LOG(EF2_LOG_PREFIX "rpc CreateThread -> %d\n", thread_id);

    if (thread_id < 0)
        return MODULE_NO_RESIDENT_END;

    start_result = StartThread(thread_id, 0);
    EF2_LOG(EF2_LOG_PREFIX "rpc StartThread -> %d\n", start_result);

    if (start_result < 0)
        return MODULE_NO_RESIDENT_END;

    EF2_LOG(
        EF2_LOG_PREFIX "waiting for rpc ready sema=%d\n",
        g_rpc_ready_sema);

    wait_result = WaitSema(g_rpc_ready_sema);

    EF2_LOG(
        EF2_LOG_PREFIX "rpc ready WaitSema -> %d\n",
        wait_result);

    EF2_LOG(EF2_LOG_PREFIX "_start returning MODULE_RESIDENT_END\n");
    return MODULE_RESIDENT_END;
}
