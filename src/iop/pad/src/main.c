#include "irx_imports.h"
#include "sio2_direct.h"
#include <ef2/pad_rpc.h>

#define EF2PAD_PORT_COUNT 2u
#define EF2PAD_MAX_PACKET 32u
#define EF2PAD_RPC_INPUT_BYTES \
    ((sizeof(ef2_pad_rpc_request) + 63u) & ~63u)

IRX_ID("ef2pad", 1, 0);

typedef struct {
    ef2_u32 frame;
    ef2_u32 buttons;
    ef2_u32 errors;
    ef2_u32 reconnects;

    ef2_u8 connected;
    ef2_u8 id;
    ef2_u8 timing_profile;
    ef2_u8 stat70_bit;

    ef2_u8 right_x;
    ef2_u8 right_y;
    ef2_u8 left_x;
    ef2_u8 left_y;

    ef2_u8 pressure[12];
} ef2pad_port_state;

static SifRpcDataQueue_t g_rpc_queue;
static SifRpcServerData_t g_rpc_server;

static unsigned char g_rpc_input[EF2PAD_RPC_INPUT_BYTES]
    __attribute__((aligned(64)));
static ef2_pad_rpc_reply g_rpc_reply
    __attribute__((aligned(64)));

static ef2pad_port_state g_ports[EF2PAD_PORT_COUNT];

static int g_rpc_ready_sema = -1;

static void clear_bytes(void *ptr, ef2_u32 size)
{
    ef2_u8 *bytes = (ef2_u8 *)ptr;
    ef2_u32 i;

    for (i = 0; i < size; ++i)
        bytes[i] = 0;
}

static ef2_u32 packet_size_for_id(ef2_u8 id)
{
    ef2_u32 size =
        ((ef2_u32)(id & 0x0Fu) * 2u) + 3u;

    if (size < 5u)
        size = 5u;
    if (size > EF2PAD_MAX_PACKET)
        size = EF2PAD_MAX_PACKET;

    return size;
}

static void shift_stat70_reply(
    ef2_u8 *buffer,
    ef2_u32 size)
{
    ef2_u32 i;

    if (size == 0u)
        return;

    for (i = size - 1u; i > 0u; --i)
        buffer[i] = buffer[i - 1u];

    buffer[0] = 0xFFu;
}

static int response_looks_like_pad(
    const ef2_u8 *buffer,
    ef2_u32 size)
{
    ef2_u8 id;

    if (size < 5u)
        return 0;

    id = buffer[1];

    if (id == 0u || id == 0xFFu)
        return 0;

    if (buffer[2] != 0x5Au)
        return 0;

    return 1;
}

static int transfer_poll(
    ef2_u32 port,
    ef2_u8 id_hint,
    ef2_u8 timing_profile,
    ef2_u8 stat70_bit,
    ef2_u8 *output,
    ef2_u32 *output_size,
    ef2_u8 *next_stat70)
{
    ef2_u8 input[EF2PAD_MAX_PACKET];
    ef2_sio2_result transfer_result = {0, 0, 0};
    ef2_u32 size;
    ef2_u32 i;
    int result;

    size = id_hint == 0u
        ? 5u
        : packet_size_for_id(id_hint);

    for (i = 0; i < EF2PAD_MAX_PACKET; ++i) {
        input[i] = 0;
        output[i] = 0;
    }

    input[0] = 1u;
    input[1] = 0x42u;
    input[2] = 0u;

    result = ef2_sio2_transfer_pad(
        port,
        timing_profile,
        stat70_bit,
        input,
        size,
        output,
        &transfer_result);

    *next_stat70 =
        (ef2_u8)((transfer_result.recv2 >>
                  (4u + port)) & 1u);

    if (result < 0)
        return result;

    if (stat70_bit != 0u)
        shift_stat70_reply(output, size);

    if (!response_looks_like_pad(output, size))
        return -10;

    *output_size = size;
    return 0;
}

static void parse_reply(
    ef2pad_port_state *state,
    const ef2_u8 *reply,
    ef2_u32 size)
{
    ef2_u32 i;
    ef2_u16 raw_buttons;

    raw_buttons =
        (ef2_u16)reply[3] |
        ((ef2_u16)reply[4] << 8);

    state->buttons =
        (ef2_u32)((~raw_buttons) & 0xFFFFu);

    state->right_x = 0x80u;
    state->right_y = 0x80u;
    state->left_x = 0x80u;
    state->left_y = 0x80u;

    for (i = 0; i < 12u; ++i)
        state->pressure[i] = 0;

    if (size >= 9u) {
        state->right_x = reply[5];
        state->right_y = reply[6];
        state->left_x = reply[7];
        state->left_y = reply[8];
    }

    if (size >= 21u) {
        for (i = 0; i < 12u; ++i)
            state->pressure[i] = reply[9u + i];
    }
}

static int try_profile(
    ef2_u32 port,
    ef2pad_port_state *state,
    ef2_u8 id_hint,
    ef2_u8 timing_profile,
    ef2_u8 stat70_bit,
    ef2_u8 *reply,
    ef2_u32 *reply_size)
{
    ef2_u8 next_stat70 = 0;
    int result;

    result = transfer_poll(
        port,
        id_hint,
        timing_profile,
        stat70_bit,
        reply,
        reply_size,
        &next_stat70);

    if (result < 0)
        return result;

    state->stat70_bit = next_stat70;
    state->timing_profile = timing_profile;
    state->id = reply[1];

    return 0;
}

static int discover_pad(
    ef2_u32 port,
    ef2pad_port_state *state,
    ef2_u8 *reply,
    ef2_u32 *reply_size)
{
    int result;

    result = try_profile(
        port,
        state,
        0,
        0,
        state->stat70_bit,
        reply,
        reply_size);

    if (result == 0)
        return 0;

    result = try_profile(
        port,
        state,
        0,
        1,
        0,
        reply,
        reply_size);

    if (result == 0)
        return 0;

    result = try_profile(
        port,
        state,
        0,
        0,
        1,
        reply,
        reply_size);

    return result;
}

static int poll_port(ef2_u32 port)
{
    ef2pad_port_state *state = &g_ports[port];
    ef2_u8 reply[EF2PAD_MAX_PACKET];
    ef2_u32 reply_size = 0;
    ef2_u8 was_connected = state->connected;
    ef2_u8 previous_id = state->id;
    int result;

    if (state->connected) {
        result = try_profile(
            port,
            state,
            state->id,
            state->timing_profile,
            state->stat70_bit,
            reply,
            &reply_size);

        if (result < 0) {
            result = try_profile(
                port,
                state,
                state->id,
                (ef2_u8)(1u - state->timing_profile),
                0,
                reply,
                &reply_size);
        }

        if (result < 0) {
            result = discover_pad(
                port,
                state,
                reply,
                &reply_size);
        }
    } else {
        result = discover_pad(
            port,
            state,
            reply,
            &reply_size);
    }

    if (result < 0) {
        ++state->errors;
        state->connected = 0;
        state->buttons = 0;
        state->id = 0;
        state->right_x = 0x80u;
        state->right_y = 0x80u;
        state->left_x = 0x80u;
        state->left_y = 0x80u;
        return result;
    }

    if (reply[1] != previous_id ||
        reply_size < packet_size_for_id(reply[1])) {
        ef2_u8 discovery_reply[EF2PAD_MAX_PACKET];
        ef2_u8 discovered_id = reply[1];
        ef2_u32 discovery_size = reply_size;
        ef2_u32 full_size = 0;
        ef2_u32 i;

        for (i = 0; i < EF2PAD_MAX_PACKET; ++i)
            discovery_reply[i] = reply[i];

        result = try_profile(
            port,
            state,
            discovered_id,
            state->timing_profile,
            state->stat70_bit,
            reply,
            &full_size);

        if (result == 0) {
            reply_size = full_size;
        } else {
            for (i = 0; i < EF2PAD_MAX_PACKET; ++i)
                reply[i] = discovery_reply[i];

            reply_size = discovery_size;
            state->id = discovered_id;
        }
    }

    state->connected = 1;
    ++state->frame;

    if (!was_connected)
        ++state->reconnects;

    parse_reply(state, reply, reply_size);
    return 0;
}

static void fill_rpc_port(
    ef2_pad_rpc_port_state *dest,
    const ef2pad_port_state *source)
{
    ef2_u32 i;

    dest->frame = source->frame;
    dest->buttons = source->buttons;
    dest->errors = source->errors;
    dest->reconnects = source->reconnects;

    dest->connected = source->connected;
    dest->raw_id = source->id;
    dest->timing_profile = source->timing_profile;
    dest->reserved0 = 0;

    dest->right_x = source->right_x;
    dest->right_y = source->right_y;
    dest->left_x = source->left_x;
    dest->left_y = source->left_y;

    for (i = 0; i < 12u; ++i)
        dest->pressure[i] = source->pressure[i];

    for (i = 0; i < 4u; ++i)
        dest->reserved1[i] = 0;
}

static void *rpc_handler(
    int function,
    void *buffer,
    int length)
{
    ef2_pad_rpc_request *request =
        (ef2_pad_rpc_request *)buffer;
    ef2_u32 port;

    g_rpc_reply.result = 0;

    switch (function) {
        case EF2_PAD_RPC_INIT:
            break;

        case EF2_PAD_RPC_POLL:
            if (length < (int)sizeof(*request)) {
                g_rpc_reply.result = -1;
                break;
            }

            for (port = 0; port < EF2PAD_PORT_COUNT; ++port) {
                if ((request->port_mask & (1u << port)) != 0u)
                    (void)poll_port(port);
            }
            break;

        default:
            g_rpc_reply.result = -100;
            break;
    }

    for (port = 0; port < EF2PAD_PORT_COUNT; ++port)
        fill_rpc_port(&g_rpc_reply.port[port], &g_ports[port]);

    return &g_rpc_reply;
}

static void rpc_thread(void *arg)
{
    int tid;

    (void)arg;

    tid = GetThreadId();

    sceSifInitRpc(0);
    sceSifSetRpcQueue(&g_rpc_queue, tid);
    sceSifRegisterRpc(
        &g_rpc_server,
        EF2_PAD_RPC_SID,
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
    iop_sema_t sema;
    ef2_u32 port;
    int thread_id;
    int sio2_result;

    (void)argc;
    (void)argv;

    CpuEnableIntr();

    for (port = 0; port < EF2PAD_PORT_COUNT; ++port) {
        clear_bytes(&g_ports[port], sizeof(g_ports[port]));
        g_ports[port].right_x = 0x80u;
        g_ports[port].right_y = 0x80u;
        g_ports[port].left_x = 0x80u;
        g_ports[port].left_y = 0x80u;
    }

    sio2_result = ef2_sio2_init();
    if (sio2_result < 0)
        return MODULE_NO_RESIDENT_END;

    sema.attr = 0;
    sema.option = 0;
    sema.initial = 0;
    sema.max = 1;

    g_rpc_ready_sema = CreateSema(&sema);
    if (g_rpc_ready_sema < 0)
        return MODULE_NO_RESIDENT_END;

    thread.attr = TH_C;
    thread.option = 0;
    thread.thread = rpc_thread;
    thread.stacksize = 0x1000;
    thread.priority = 39;

    thread_id = CreateThread(&thread);
    if (thread_id < 0)
        return MODULE_NO_RESIDENT_END;

    if (StartThread(thread_id, 0) < 0)
        return MODULE_NO_RESIDENT_END;

    WaitSema(g_rpc_ready_sema);
    return MODULE_RESIDENT_END;
}
