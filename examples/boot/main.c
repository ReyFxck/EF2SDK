#include <ef2/audio.h>
#include <ef2/base.h>
#include <ef2/pad.h>
#include <ef2/video.h>

#define EF2_MELODY_RATE 32000u
#define EF2_MELODY_NOTE_FRAMES 8000u
#define EF2_MELODY_INPUT_FRAMES 513u
#define EF2_MELODY_OUTPUT_FRAMES 800u
#define EF2_PAD_TEST_DEADZONE 12u
#define EF2_PAD_HOLD_STEP_POLLS 8u

volatile ef2_u32 ef2_boot_counter;
volatile ef2_s32 ef2_video_status;
volatile ef2_s32 ef2_audio_status;

static ef2_s16 g_melody_input[EF2_MELODY_INPUT_FRAMES * 2u];
static ef2_s16 g_melody_output[EF2_MELODY_OUTPUT_FRAMES * 2u];
static ef2_u32 g_checkerboard[32u * 32u] EF2_ALIGN(16);
static ef2_u8 g_indexed_pixels[32u * 32u] EF2_ALIGN(16);
static ef2_u32 g_indexed_palette[256] EF2_ALIGN(16);
static ef2_u8 g_indexed4_linear[32u * 32u] EF2_ALIGN(16);
static ef2_u8 g_indexed4_packed[(32u * 32u) / 2u] EF2_ALIGN(16);
static ef2_u32 g_indexed4_palette[16] EF2_ALIGN(16);

static const ef2_u16 g_melody_notes[] = {
    262, 330, 392, 523,
    440, 349, 392, 294,
    330, 494, 392, 262,
    294, 349, 440, 0
};

static ef2_s16 melody_sample(ef2_u32 absolute_frame)
{
    const ef2_u32 note_count =
        sizeof(g_melody_notes) / sizeof(g_melody_notes[0]);
    ef2_u32 note_index =
        (absolute_frame / EF2_MELODY_NOTE_FRAMES) % note_count;
    ef2_u32 frame_in_note =
        absolute_frame % EF2_MELODY_NOTE_FRAMES;
    ef2_u32 frequency = g_melody_notes[note_index];
    ef2_u32 phase;
    ef2_s32 sample;
    ef2_u32 envelope = 256u;

    if (frequency == 0)
        return 0;

    phase = (frame_in_note * frequency) % EF2_MELODY_RATE;

    if (phase < 8000u)
        sample = -9000 + (ef2_s32)((phase * 18000u) / 8000u);
    else if (phase < 16000u)
        sample = 9000 - (ef2_s32)(((phase - 8000u) * 18000u) / 8000u);
    else if (phase < 24000u)
        sample = -9000 + (ef2_s32)(((phase - 16000u) * 18000u) / 8000u);
    else
        sample = 9000 - (ef2_s32)(((phase - 24000u) * 18000u) / 8000u);

    if (frame_in_note < 256u)
        envelope = frame_in_note;
    else if (frame_in_note > EF2_MELODY_NOTE_FRAMES - 257u)
        envelope = EF2_MELODY_NOTE_FRAMES - 1u - frame_in_note;

    sample = (sample * (ef2_s32)envelope) / 256;
    return (ef2_s16)sample;
}

static void generate_checkerboard(void)
{
    ef2_u32 y;

    for (y = 0; y < 32u; ++y) {
        ef2_u32 x;

        for (x = 0; x < 32u; ++x) {
            ef2_u32 checker =
                ((x >> 3) ^ (y >> 3)) & 1u;

            g_checkerboard[y * 32u + x] =
                checker
                    ? 0x80F05020u
                    : 0x802040F0u;
        }
    }
}

static void generate_indexed_texture(void)
{
    ef2_u32 i;
    ef2_u32 y;

    for (i = 0; i < 256u; ++i) {
        ef2_u8 r = (ef2_u8)i;
        ef2_u8 g = (ef2_u8)(255u - i);
        ef2_u8 b = (ef2_u8)((i * 5u) & 0xFFu);

        g_indexed_palette[i] =
            (ef2_u32)r |
            ((ef2_u32)g << 8) |
            ((ef2_u32)b << 16) |
            (0x80u << 24);
    }

    for (y = 0; y < 32u; ++y) {
        ef2_u32 x;

        for (x = 0; x < 32u; ++x) {
            g_indexed_pixels[y * 32u + x] =
                (ef2_u8)(
                    x * 8u +
                    (y & 7u));
        }
    }
}

static void generate_indexed4_texture(void)
{
    static const ef2_u32 colors[16] = {
        0x80000000u, 0x800000F0u,
        0x8000F000u, 0x8000F0F0u,
        0x80F00000u, 0x80F000F0u,
        0x80F0F000u, 0x80F0F0F0u,
        0x80404040u, 0x804040F0u,
        0x8040F040u, 0x8040F0F0u,
        0x80F04040u, 0x80F040F0u,
        0x80F0F040u, 0x80FFFFFFu
    };
    ef2_u32 i;
    ef2_u32 y;

    for (i = 0; i < 16u; ++i)
        g_indexed4_palette[i] = colors[i];

    for (y = 0; y < 32u; ++y) {
        ef2_u32 x;

        for (x = 0; x < 32u; ++x) {
            g_indexed4_linear[y * 32u + x] =
                (ef2_u8)(
                    ((x >> 2) +
                     (y >> 2) * 3u) & 0x0Fu);
        }
    }

    (void)ef2_video_pack_indices4(
        g_indexed4_packed,
        sizeof(g_indexed4_packed),
        g_indexed4_linear,
        32u * 32u);
}

static void show_gif_transport_indicator(void)
{
    ef2_video_transport_stats stats;

    if (ef2_video_get_transport_stats(
            &stats) != 0)
        return;

    if (stats.dma_available &&
        stats.dma_fallbacks == 0u) {
        (void)ef2_video_draw_rect(
            128, 16, 32, 32,
            32, 224, 80);
    } else {
        (void)ef2_video_draw_rect(
            128, 16, 32, 32,
            232, 112, 24);
    }

    /*
     * Re-read after drawing the indicator itself. If that packet caused
     * the first DMA failure, overwrite the square in orange via FIFO.
     */
    if (ef2_video_get_transport_stats(
            &stats) == 0 &&
        (!stats.dma_available ||
         stats.dma_fallbacks != 0u)) {
        (void)ef2_video_draw_rect(
            128, 16, 32, 32,
            232, 112, 24);
    }
}

static void generate_melody_window(ef2_u32 source_frame)
{
    ef2_u32 i;

    for (i = 0; i < EF2_MELODY_INPUT_FRAMES; ++i) {
        ef2_s16 sample = melody_sample(source_frame + i);
        g_melody_input[i * 2u] = sample;
        g_melody_input[i * 2u + 1u] = sample;
    }
}


static void show_audio_state(
    ef2_s32 paused,
    ef2_s32 stopped)
{
    if (stopped)
        ef2_video_clear(112, 40, 48);
    else if (paused)
        ef2_video_clear(32, 96, 224);
    else
        ef2_video_clear(24, 176, 120);
}

static int show_analog_state(
    const ef2_pad_state *pad)
{
    ef2_s16 lx;
    ef2_s16 ly;
    ef2_s16 rx;
    ef2_s16 ry;
    ef2_u32 brightness;
    ef2_u32 red;
    ef2_u32 green;
    ef2_u32 blue;

    if (!ef2_pad_has_analog(pad))
        return 0;

    lx = ef2_pad_axis_deadzone(
        pad->left_x,
        EF2_PAD_TEST_DEADZONE);
    ly = ef2_pad_axis_deadzone(
        pad->left_y,
        EF2_PAD_TEST_DEADZONE);
    rx = ef2_pad_axis_deadzone(
        pad->right_x,
        EF2_PAD_TEST_DEADZONE);
    ry = ef2_pad_axis_deadzone(
        pad->right_y,
        EF2_PAD_TEST_DEADZONE);

    if (lx == 0 && ly == 0 &&
        rx == 0 && ry == 0)
        return 0;

    /*
     * Four-axis visual diagnostic:
     * left X -> red, left Y -> green,
     * right X -> blue, right Y -> brightness.
     */
    brightness =
        64u +
        (((ef2_u32)(255u - pad->right_y) *
          191u) / 255u);

    red =
        ((ef2_u32)pad->left_x *
         brightness) / 255u;
    green =
        ((ef2_u32)(255u - pad->left_y) *
         brightness) / 255u;
    blue =
        ((ef2_u32)pad->right_x *
         brightness) / 255u;

    ef2_video_clear(
        (ef2_u8)red,
        (ef2_u8)green,
        (ef2_u8)blue);

    return 1;
}

static void show_init_failure(ef2_s32 status)
{
    if (status <= -4000) {
        /* Red: RPC bound; SPU2/libsd initialization inside ef2audio failed. */
        ef2_video_clear(220, 40, 48);
    } else if (status <= -3000) {
        /* Orange: IRX stayed resident but its RPC SID could not be bound. */
        ef2_video_clear(232, 112, 24);
    } else if (status <= -2800) {
        /* Blue-violet: IRX _start returned non-resident (modres != 0). */
        ef2_video_clear(88, 72, 216);
    } else if (status <= -2000) {
        /* Purple: embedded IRX LoadModuleBuffer/patch path failed. */
        ef2_video_clear(168, 48, 208);
    } else if (status <= -1000) {
        /* Yellow: base SIFCMD/RPC initialization failed. */
        ef2_video_clear(232, 200, 32);
    } else {
        /* White: unexpected non-stage error. */
        ef2_video_clear(224, 224, 224);
    }
}

int main(void)
{
    const ef2_video_config video = {
        .standard = EF2_VIDEO_NTSC,
        .interlaced = 1,
        .field_mode = EF2_VIDEO_FIELD,
    };
    ef2_audio_rate_converter converter;
    ef2_u32 source_frame = 0;
    ef2_u32 submitted_before_start = 0;
    ef2_s32 audio_started = 0;
    ef2_s32 audio_paused = 0;
    ef2_s32 audio_stopped = 0;
    ef2_u32 audio_volume = 0x3000u;
    ef2_pad_state pad;
    ef2_pad_state pads[EF2_PAD_PORT_COUNT];
    ef2_s32 analog_visual_active = 0;
    ef2_u8 rumble_small = 0;
    ef2_u8 rumble_large = 0;
    ef2_video_texture checker_texture;
    ef2_video_texture indexed_texture;
    ef2_video_texture indexed4_texture;

    ef2_boot_counter = 1;
    ef2_audio_status = -1;

    ef2_video_status = ef2_video_init(&video);
    if (ef2_video_status != 0) {
        for (;;)
            ++ef2_boot_counter;
    }

    ef2_video_clear(32, 96, 224);

    /* Initial primitive/DMA smoke: clipped rect + diagonal line. */
    (void)ef2_video_draw_rect(
        16,
        16,
        96,
        48,
        232,
        200,
        32);

    (void)ef2_video_draw_line(
        16,
        72,
        112,
        16,
        224,
        224,
        224);

    generate_checkerboard();

    if (ef2_video_upload_rgba32(
            &checker_texture,
            g_checkerboard,
            32,
            32) == 0) {
        (void)ef2_video_draw_texture(
            &checker_texture,
            176,
            16,
            96,
            96);
    } else {
        (void)ef2_video_draw_rect(
            176,
            16,
            96,
            96,
            220,
            40,
            48);
    }

    /*
     * Crop/UV + alpha smoke:
     * draw the center 16x16 portion over a yellow base at 50% opacity.
     */
    (void)ef2_video_draw_rect(
        304,
        16,
        96,
        96,
        232,
        200,
        32);

    (void)ef2_video_draw_texture_region(
        &checker_texture,
        8,
        8,
        16,
        16,
        320,
        32,
        64,
        64,
        0x80,
        0x80,
        0x80,
        0x40,
        1);

    generate_indexed_texture();

    if (ef2_video_upload_indexed8(
            &indexed_texture,
            g_indexed_pixels,
            g_indexed_palette,
            32,
            32) == 0) {
        (void)ef2_video_draw_texture(
            &indexed_texture,
            416,
            16,
            96,
            96);
    } else {
        (void)ef2_video_draw_rect(
            416,
            16,
            96,
            96,
            220,
            40,
            48);
    }

    generate_indexed4_texture();

    if (ef2_video_upload_indexed4(
            &indexed4_texture,
            g_indexed4_packed,
            g_indexed4_palette,
            32,
            32) == 0) {
        (void)ef2_video_draw_texture(
            &indexed4_texture,
            528,
            16,
            96,
            96);
    } else {
        (void)ef2_video_draw_rect(
            528,
            16,
            96,
            96,
            220,
            40,
            48);
    }

    show_gif_transport_indicator();

    ef2_audio_status = ef2_audio_device_init();
    if (ef2_audio_status != 0) {
        show_init_failure(ef2_audio_status);
        for (;;)
            ++ef2_boot_counter;
    }

    if (ef2_audio_device_set_latency_ms(43u) != 0 ||
        ef2_audio_device_set_volume(audio_volume) != 0) {
        ef2_video_clear(220, 40, 48);
        for (;;)
            ++ef2_boot_counter;
    }

    if (ef2_pad_init() != 0) {
        ef2_video_clear(176, 48, 208);
        for (;;)
            ++ef2_boot_counter;
    }

    if (ef2_audio_rate_converter_init(
            &converter,
            EF2_MELODY_RATE,
            48000,
            2,
            EF2_AUDIO_RESAMPLE_LINEAR) != 0) {
        ef2_video_clear(220, 40, 48);
        for (;;)
            ++ef2_boot_counter;
    }

    for (;;) {
        ef2_u32 consumed = 0;
        ef2_u32 produced = 0;

        if (ef2_pad_poll_all(pads) == 0) {
            pad = pads[0];

            if (pads[1].connected &&
                ef2_pad_is_held(
                    &pads[1],
                    EF2_PAD_CIRCLE)) {
                ef2_video_clear(160, 64, 224);
                analog_visual_active = 1;
            }

            if (pad.connected) {
            if (ef2_pad_was_pressed(
                    &pad,
                    EF2_PAD_CROSS)) {
                if (audio_paused) {
                    if (ef2_audio_device_resume() == 0) {
                        audio_paused = 0;
                        show_audio_state(
                            audio_paused,
                            audio_stopped);
                    }
                } else if (!audio_stopped) {
                    if (ef2_audio_device_pause() == 0) {
                        audio_paused = 1;
                        show_audio_state(
                            audio_paused,
                            audio_stopped);
                    }
                }
            }

            if (ef2_pad_was_pressed(
                    &pad,
                    EF2_PAD_START)) {
                if (audio_stopped) {
                    if (ef2_audio_device_start() == 0) {
                        audio_stopped = 0;
                        audio_paused = 0;
                        show_audio_state(
                            audio_paused,
                            audio_stopped);
                    }
                } else {
                    if (ef2_audio_device_stop() == 0) {
                        audio_stopped = 1;
                        audio_paused = 0;
                        show_audio_state(
                            audio_paused,
                            audio_stopped);
                    }
                }
            }

            if (ef2_pad_was_pressed(
                    &pad,
                    EF2_PAD_SQUARE))
                (void)ef2_audio_device_flush();

            /*
             * Held-button diagnostic: keeping the D-pad direction
             * down repeats the volume step every few successful polls.
             */
            if ((pad.frame %
                 EF2_PAD_HOLD_STEP_POLLS) == 0u) {
                if (ef2_pad_is_held(
                        &pad,
                        EF2_PAD_UP)) {
                    if (audio_volume <=
                        EF2_AUDIO_VOLUME_MAX - 0x0100u)
                        audio_volume += 0x0100u;
                    else
                        audio_volume =
                            EF2_AUDIO_VOLUME_MAX;

                    (void)ef2_audio_device_set_volume(
                        audio_volume);
                }

                if (ef2_pad_is_held(
                        &pad,
                        EF2_PAD_DOWN)) {
                    if (audio_volume >= 0x0100u)
                        audio_volume -= 0x0100u;
                    else
                        audio_volume = 0;

                    (void)ef2_audio_device_set_volume(
                        audio_volume);
                }
            }

            {
                ef2_u8 desired_small =
                    ef2_pad_is_held(
                        &pad,
                        EF2_PAD_R1) ? 1u : 0u;
                ef2_u8 desired_large = 0;

                if (ef2_pad_is_held(
                        &pad,
                        EF2_PAD_R2)) {
                    if (ef2_pad_has_pressure(&pad) &&
                        pad.pressure_r2 != 0u)
                        desired_large = pad.pressure_r2;
                    else
                        desired_large = 0xFFu;
                }

                if (desired_small != rumble_small ||
                    desired_large != rumble_large) {
                    if (ef2_pad_set_rumble(
                            0,
                            desired_small,
                            desired_large) == 0) {
                        rumble_small = desired_small;
                        rumble_large = desired_large;
                    }
                }
            }

            if (show_analog_state(&pad)) {
                analog_visual_active = 1;
            } else if (analog_visual_active &&
                       !pads[1].connected) {
                analog_visual_active = 0;
                show_audio_state(
                    audio_paused,
                    audio_stopped);
            }
            }
        }

        if (audio_paused || audio_stopped) {
            ++ef2_boot_counter;
            continue;
        }

        generate_melody_window(source_frame);

        produced = ef2_audio_rate_converter_process_s16(
            &converter,
            g_melody_input,
            EF2_MELODY_INPUT_FRAMES,
            g_melody_output,
            EF2_MELODY_OUTPUT_FRAMES,
            &consumed);

        source_frame += consumed;

        if (produced != 0) {
            if (ef2_audio_device_submit_48k_stereo_s16(
                    g_melody_output,
                    produced) < 0) {
                ef2_video_clear(220, 40, 48);
                for (;;)
                    ++ef2_boot_counter;
            }

            submitted_before_start += produced;
        }

        if (!audio_started && submitted_before_start >= 2048u) {
            ef2_audio_status = ef2_audio_device_start();

            if (ef2_audio_status != 0) {
                ef2_video_clear(220, 40, 48);
                for (;;)
                    ++ef2_boot_counter;
            }

            audio_started = 1;
            show_audio_state(
                audio_paused,
                audio_stopped);
        }

        ++ef2_boot_counter;
    }
}
