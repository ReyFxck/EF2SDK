#include <ef2/pad.h>
#include <ef2/pad_rpc.h>
#include <ef2/sif.h>

extern const ef2_u8 ef2pad_irx[];
extern const ef2_u32 ef2pad_irx_size;

static ef2_sif_rpc_client g_pad_client;
static ef2_pad_rpc_request g_request EF2_ALIGN(64);
static ef2_pad_rpc_reply g_reply EF2_ALIGN(64);

static ef2_u32
    g_previous_buttons[EF2_PAD_PORT_COUNT][EF2_PAD_SLOT_COUNT];
static ef2_u32 g_slot_count[EF2_PAD_PORT_COUNT] = {1u, 1u};
static ef2_s32 g_pad_bound;

static void zero_bytes(void *ptr, ef2_u32 size)
{
    ef2_u8 *bytes = (ef2_u8 *)ptr;
    ef2_u32 i;

    for (i = 0; i < size; ++i)
        bytes[i] = 0;
}

static int pad_rpc_exchange(ef2_s32 function)
{
    int result;

    zero_bytes(&g_reply, sizeof(g_reply));

    result = ef2_sif_call(
        &g_pad_client,
        function,
        &g_request,
        sizeof(g_request),
        &g_reply,
        sizeof(g_reply));

    if (result < 0)
        return result;

    g_slot_count[0] =
        g_reply.slot_count[0] == 0u ? 1u : g_reply.slot_count[0];
    g_slot_count[1] =
        g_reply.slot_count[1] == 0u ? 1u : g_reply.slot_count[1];

    return g_reply.result;
}

static int pad_rpc_call(
    ef2_s32 function,
    ef2_u32 port_mask)
{
    zero_bytes(&g_request, sizeof(g_request));
    g_request.port_mask = port_mask;

    return pad_rpc_exchange(function);
}

static void copy_state(
    ef2_u32 port,
    ef2_u32 slot,
    ef2_pad_state *dest,
    const ef2_pad_rpc_port_state *source)
{
    ef2_u32 current_buttons;
    ef2_u32 previous_buttons;

    current_buttons = source->connected
        ? source->buttons
        : 0u;

    previous_buttons = g_previous_buttons[port][slot];

    dest->frame = source->frame;
    dest->buttons = current_buttons;
    dest->pressed =
        current_buttons & ~previous_buttons;
    dest->released =
        previous_buttons & ~current_buttons;
    dest->errors = source->errors;
    dest->reconnects = source->reconnects;

    dest->connected = source->connected;
    dest->raw_id = source->raw_id;
    dest->mode = (ef2_u8)(source->raw_id >> 4);
    dest->timing_profile = source->timing_profile;
    dest->rumble_supported = source->rumble_supported;
    dest->rumble_small = source->rumble_small;
    dest->rumble_large = source->rumble_large;
    dest->reserved0 = 0;

    dest->right_x = source->right_x;
    dest->right_y = source->right_y;
    dest->left_x = source->left_x;
    dest->left_y = source->left_y;

    dest->pressure_right = source->pressure[0];
    dest->pressure_left = source->pressure[1];
    dest->pressure_up = source->pressure[2];
    dest->pressure_down = source->pressure[3];
    dest->pressure_triangle = source->pressure[4];
    dest->pressure_circle = source->pressure[5];
    dest->pressure_cross = source->pressure[6];
    dest->pressure_square = source->pressure[7];
    dest->pressure_l1 = source->pressure[8];
    dest->pressure_r1 = source->pressure[9];
    dest->pressure_l2 = source->pressure[10];
    dest->pressure_r2 = source->pressure[11];

    g_previous_buttons[port][slot] = current_buttons;
}

int ef2_pad_init(void)
{
    ef2_s32 module_result = -1;
    ef2_u32 port;
    ef2_u32 slot;
    int result;

    if (g_pad_bound)
        return 0;

    result = ef2_sif_init();
    if (result < 0)
        return -1000 + result;

    result = ef2_iop_exec_module_buffer_ex(
        ef2pad_irx,
        ef2pad_irx_size,
        &module_result);

    if (result < 0) {
        int patch_result =
            ef2_iop_enable_module_buffer();

        if (patch_result < 0)
            return -2000 + patch_result;

        module_result = -1;
        result = ef2_iop_exec_module_buffer_ex(
            ef2pad_irx,
            ef2pad_irx_size,
            &module_result);

        if (result < 0)
            return -2500 + result;
    }

    if (module_result != 0)
        return -2800 - (module_result & 0xFF);

    zero_bytes(&g_pad_client, sizeof(g_pad_client));

    result = ef2_sif_bind(
        &g_pad_client,
        EF2_PAD_RPC_SID);

    if (result < 0 ||
        g_pad_client.server == (void *)0)
        return -3000 +
            ((result < 0) ? result : -9);

    g_pad_bound = 1;

    for (port = 0; port < EF2_PAD_PORT_COUNT; ++port) {
        g_slot_count[port] = 1u;

        for (slot = 0; slot < EF2_PAD_SLOT_COUNT; ++slot)
            g_previous_buttons[port][slot] = 0;
    }

    result = pad_rpc_call(EF2_PAD_RPC_INIT, 0);
    if (result < 0) {
        g_pad_bound = 0;
        return -4000 + result;
    }

    return ef2_pad_refresh_topology();
}

int ef2_pad_refresh_topology(void)
{
    if (!g_pad_bound)
        return -1;

    zero_bytes(&g_request, sizeof(g_request));
    return pad_rpc_exchange(EF2_PAD_RPC_REFRESH_TOPOLOGY);
}

int ef2_pad_get_slot_count(
    ef2_u32 port,
    ef2_u32 *slot_count)
{
    if (!g_pad_bound ||
        port >= EF2_PAD_PORT_COUNT ||
        slot_count == (ef2_u32 *)0)
        return -1;

    *slot_count = g_slot_count[port];
    return 0;
}

int ef2_pad_poll_slot(
    ef2_u32 port,
    ef2_u32 slot,
    ef2_pad_state *state)
{
    int result;

    if (!g_pad_bound ||
        port >= EF2_PAD_PORT_COUNT ||
        slot >= EF2_PAD_SLOT_COUNT ||
        state == (ef2_pad_state *)0)
        return -1;

    if (slot >= g_slot_count[port])
        return -2;

    zero_bytes(&g_request, sizeof(g_request));
    g_request.port = (ef2_u8)port;
    g_request.slot = (ef2_u8)slot;

    result = pad_rpc_exchange(EF2_PAD_RPC_POLL_SLOT);
    if (result < 0)
        return result;

    copy_state(
        port,
        slot,
        state,
        &g_reply.slot_state);

    return 0;
}

int ef2_pad_poll(
    ef2_u32 port,
    ef2_pad_state *state)
{
    return ef2_pad_poll_slot(port, 0, state);
}

int ef2_pad_poll_all(
    ef2_pad_state states[EF2_PAD_PORT_COUNT])
{
    ef2_u32 port;
    int result;

    if (!g_pad_bound ||
        states == (ef2_pad_state *)0)
        return -1;

    result = pad_rpc_call(
        EF2_PAD_RPC_POLL,
        (1u << EF2_PAD_PORT_COUNT) - 1u);

    if (result < 0)
        return result;

    for (port = 0; port < EF2_PAD_PORT_COUNT; ++port) {
        copy_state(
            port,
            0,
            &states[port],
            &g_reply.port[port]);
    }

    return 0;
}

int ef2_pad_set_rumble_slot(
    ef2_u32 port,
    ef2_u32 slot,
    ef2_u8 small_motor,
    ef2_u8 large_motor)
{
    if (!g_pad_bound ||
        port >= EF2_PAD_PORT_COUNT ||
        slot >= EF2_PAD_SLOT_COUNT ||
        slot >= g_slot_count[port] ||
        small_motor > 1u)
        return -1;

    zero_bytes(&g_request, sizeof(g_request));
    g_request.port = (ef2_u8)port;
    g_request.slot = (ef2_u8)slot;
    g_request.small_motor = small_motor;
    g_request.large_motor = large_motor;

    return pad_rpc_exchange(EF2_PAD_RPC_SET_RUMBLE_SLOT);
}

int ef2_pad_stop_rumble_slot(
    ef2_u32 port,
    ef2_u32 slot)
{
    return ef2_pad_set_rumble_slot(
        port,
        slot,
        0,
        0);
}

int ef2_pad_set_rumble(
    ef2_u32 port,
    ef2_u8 small_motor,
    ef2_u8 large_motor)
{
    return ef2_pad_set_rumble_slot(
        port,
        0,
        small_motor,
        large_motor);
}

int ef2_pad_stop_rumble(ef2_u32 port)
{
    return ef2_pad_stop_rumble_slot(port, 0);
}
