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
    ef2_s32 analog_visual_active = 0;

    ef2_boot_counter = 1;
    ef2_audio_status = -1;

    ef2_video_status = ef2_video_init(&video);
    if (ef2_video_status != 0) {
        for (;;)
            ++ef2_boot_counter;
    }

    ef2_video_clear(32, 96, 224);

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

        if (ef2_pad_poll(0, &pad) == 0 && pad.connected) {
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

            if (show_analog_state(&pad)) {
                analog_visual_active = 1;
            } else if (analog_visual_active) {
                analog_visual_active = 0;
                show_audio_state(
                    audio_paused,
                    audio_stopped);
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
