#include <ef2/pad.h>

#include <stdio.h>

static int expect(int condition, const char *name)
{
    if (condition)
        return 0;

    fprintf(stderr, "pad test failed: %s\n", name);
    return 1;
}

int main(void)
{
    ef2_pad_state state = {0};
    int failed = 0;

    state.connected = 1;
    state.raw_id = 0x79u;
    state.rumble_supported = 1u;
    state.buttons = EF2_PAD_CROSS | EF2_PAD_UP;
    state.pressed = EF2_PAD_CROSS;
    state.released = EF2_PAD_SQUARE;

    failed |= expect(
        ef2_pad_is_held(&state, EF2_PAD_CROSS),
        "held cross");
    failed |= expect(
        ef2_pad_is_held(
            &state,
            EF2_PAD_CROSS | EF2_PAD_UP),
        "held combo");
    failed |= expect(
        !ef2_pad_is_held(&state, EF2_PAD_CIRCLE),
        "circle not held");
    failed |= expect(
        ef2_pad_was_pressed(&state, EF2_PAD_CROSS),
        "cross pressed");
    failed |= expect(
        ef2_pad_was_released(&state, EF2_PAD_SQUARE),
        "square released");

    failed |= expect(
        ef2_pad_has_analog(&state),
        "0x79 analog");
    failed |= expect(
        ef2_pad_has_pressure(&state),
        "0x79 pressure");
    failed |= expect(
        ef2_pad_has_rumble(&state),
        "rumble capability");

    state.raw_id = 0x73u;
    failed |= expect(
        ef2_pad_has_analog(&state),
        "0x73 analog");
    failed |= expect(
        !ef2_pad_has_pressure(&state),
        "0x73 no pressure");

    state.raw_id = 0x41u;
    failed |= expect(
        !ef2_pad_has_analog(&state),
        "0x41 digital");

    failed |= expect(
        ef2_pad_axis_deadzone(128u, 12u) == 0,
        "axis center");
    failed |= expect(
        ef2_pad_axis_deadzone(132u, 12u) == 0,
        "axis deadzone positive");
    failed |= expect(
        ef2_pad_axis_deadzone(120u, 12u) == 0,
        "axis deadzone negative");
    failed |= expect(
        ef2_pad_axis_deadzone(0u, 12u) == -32768,
        "axis negative endpoint");
    failed |= expect(
        ef2_pad_axis_deadzone(255u, 12u) == 32767,
        "axis positive endpoint");
    failed |= expect(
        ef2_pad_axis_deadzone(64u, 12u) < 0,
        "axis negative direction");
    failed |= expect(
        ef2_pad_axis_deadzone(192u, 12u) > 0,
        "axis positive direction");

    return failed ? 1 : 0;
}
