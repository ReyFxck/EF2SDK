#include <ef2/pad.h>

static int valid_mask(
    const ef2_pad_state *state,
    ef2_u32 buttons)
{
    return state != (const ef2_pad_state *)0 &&
           buttons != 0u;
}

int ef2_pad_is_held(
    const ef2_pad_state *state,
    ef2_u32 buttons)
{
    if (!valid_mask(state, buttons))
        return 0;

    return (state->buttons & buttons) == buttons;
}

int ef2_pad_was_pressed(
    const ef2_pad_state *state,
    ef2_u32 buttons)
{
    if (!valid_mask(state, buttons))
        return 0;

    return (state->pressed & buttons) == buttons;
}

int ef2_pad_was_released(
    const ef2_pad_state *state,
    ef2_u32 buttons)
{
    if (!valid_mask(state, buttons))
        return 0;

    return (state->released & buttons) == buttons;
}

int ef2_pad_has_analog(
    const ef2_pad_state *state)
{
    if (state == (const ef2_pad_state *)0 ||
        !state->connected)
        return 0;

    return (state->raw_id & 0x0Fu) >= 3u;
}

int ef2_pad_has_pressure(
    const ef2_pad_state *state)
{
    if (state == (const ef2_pad_state *)0 ||
        !state->connected)
        return 0;

    return (state->raw_id & 0x0Fu) >= 9u;
}

int ef2_pad_has_rumble(
    const ef2_pad_state *state)
{
    return state != (const ef2_pad_state *)0 &&
           state->connected &&
           state->rumble_supported != 0u;
}

ef2_s16 ef2_pad_axis_deadzone(
    ef2_u8 raw,
    ef2_u8 deadzone)
{
    ef2_s32 delta =
        (ef2_s32)raw - 128;

    if (deadzone >= 127u)
        return 0;

    if (delta > (ef2_s32)deadzone) {
        ef2_s32 value =
            (delta - (ef2_s32)deadzone) *
            32767;

        value /= 127 - (ef2_s32)deadzone;
        return (ef2_s16)value;
    }

    if (delta < -(ef2_s32)deadzone) {
        ef2_s32 value =
            (delta + (ef2_s32)deadzone) *
            32768;

        value /= 128 - (ef2_s32)deadzone;
        return (ef2_s16)value;
    }

    return 0;
}
