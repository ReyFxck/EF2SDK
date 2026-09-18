#ifndef EF2_PAD_H
#define EF2_PAD_H

#include <ef2/base.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EF2_PAD_PORT_COUNT 2u

#define EF2_PAD_SELECT   0x0001u
#define EF2_PAD_L3       0x0002u
#define EF2_PAD_R3       0x0004u
#define EF2_PAD_START    0x0008u
#define EF2_PAD_UP       0x0010u
#define EF2_PAD_RIGHT    0x0020u
#define EF2_PAD_DOWN     0x0040u
#define EF2_PAD_LEFT     0x0080u
#define EF2_PAD_L2       0x0100u
#define EF2_PAD_R2       0x0200u
#define EF2_PAD_L1       0x0400u
#define EF2_PAD_R1       0x0800u
#define EF2_PAD_TRIANGLE 0x1000u
#define EF2_PAD_CIRCLE   0x2000u
#define EF2_PAD_CROSS    0x4000u
#define EF2_PAD_SQUARE   0x8000u

typedef enum {
    EF2_PAD_MODE_UNKNOWN = 0,
    EF2_PAD_MODE_MOUSE = 1,
    EF2_PAD_MODE_NEGICON = 2,
    EF2_PAD_MODE_KONAMI_GUN = 3,
    EF2_PAD_MODE_DIGITAL = 4,
    EF2_PAD_MODE_ANALOG = 5,
    EF2_PAD_MODE_NAMCO_GUN = 6,
    EF2_PAD_MODE_DUALSHOCK = 7,
    EF2_PAD_MODE_JOGCON = 14
} ef2_pad_mode;

typedef struct {
    ef2_u32 frame;
    ef2_u32 buttons;
    ef2_u32 pressed;
    ef2_u32 released;
    ef2_u32 errors;
    ef2_u32 reconnects;

    ef2_u8 connected;
    ef2_u8 raw_id;
    ef2_u8 mode;
    ef2_u8 timing_profile;

    ef2_u8 right_x;
    ef2_u8 right_y;
    ef2_u8 left_x;
    ef2_u8 left_y;

    ef2_u8 pressure_right;
    ef2_u8 pressure_left;
    ef2_u8 pressure_up;
    ef2_u8 pressure_down;
    ef2_u8 pressure_triangle;
    ef2_u8 pressure_circle;
    ef2_u8 pressure_cross;
    ef2_u8 pressure_square;
    ef2_u8 pressure_l1;
    ef2_u8 pressure_r1;
    ef2_u8 pressure_l2;
    ef2_u8 pressure_r2;
} ef2_pad_state;

int ef2_pad_init(void);

int ef2_pad_poll(
    ef2_u32 port,
    ef2_pad_state *state);

int ef2_pad_poll_all(
    ef2_pad_state states[EF2_PAD_PORT_COUNT]);

/*
 * Button convenience helpers.
 * A multi-button mask only matches when every requested bit matches.
 */
int ef2_pad_is_held(
    const ef2_pad_state *state,
    ef2_u32 buttons);

int ef2_pad_was_pressed(
    const ef2_pad_state *state,
    ef2_u32 buttons);

int ef2_pad_was_released(
    const ef2_pad_state *state,
    ef2_u32 buttons);

/* Report capabilities from the current raw controller packet ID. */
int ef2_pad_has_analog(
    const ef2_pad_state *state);

int ef2_pad_has_pressure(
    const ef2_pad_state *state);

/*
 * Convert a raw 0..255 stick axis to signed -32768..32767.
 * Values within the symmetric center deadzone return zero.
 */
ef2_s16 ef2_pad_axis_deadzone(
    ef2_u8 raw,
    ef2_u8 deadzone);

#ifdef __cplusplus
}
#endif

#endif
