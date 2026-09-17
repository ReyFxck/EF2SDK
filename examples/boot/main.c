#include <ef2/base.h>
#include <ef2/video.h>

volatile ef2_u32 ef2_boot_counter;
volatile ef2_s32 ef2_video_status;

int main(void)
{
    const ef2_video_config video = {
        .standard = EF2_VIDEO_NTSC,
        .interlaced = 1,
        .field_mode = EF2_VIDEO_FIELD,
    };

    ef2_boot_counter = 1;
    ef2_video_status = ef2_video_init(&video);

    if (ef2_video_status == 0) {
        /* Intentionally vivid so a successful boot is unmistakable. */
        ef2_video_set_background(32, 96, 224);
    }

    for (;;) {
        ++ef2_boot_counter;
        __asm__ volatile ("nop");
    }
}
