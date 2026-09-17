#include <ef2/gs.h>
#include <ef2/kernel.h>
#include <ef2/video.h>

static int ef2_video_standard_valid(ef2_video_standard standard)
{
    return standard == EF2_VIDEO_NTSC || standard == EF2_VIDEO_PAL;
}

void ef2_video_set_background(ef2_u8 r, ef2_u8 g, ef2_u8 b)
{
    *EF2_GS_REG_BGCOLOR = ef2_gs_pack_bgcolor(r, g, b);
    ef2_gs_sync();
}

void ef2_video_output_background(void)
{
    /* With both rectangular read circuits disabled, the PCRTC falls back to
     * BGCOLOR. Keeping SLBG set also makes the intended merge source explicit.
     * This gives us a visible smoke test without a framebuffer or GIF DMA. */
    *EF2_GS_REG_PMODE = ef2_gs_pack_pmode(0, 0, 1, 0, 1, 0x80);
    ef2_gs_sync();
}

int ef2_video_init(const ef2_video_config *config)
{
    if (config == (const ef2_video_config *)0) {
        return -1;
    }

    if (!ef2_video_standard_valid(config->standard)) {
        return -2;
    }

    /* Blank output while the GS and CRT mode are being changed. */
    *EF2_GS_REG_PMODE = 0;
    ef2_gs_sync();

    /* Reset the GS before asking the EE kernel to program the PCRTC timings. */
    *EF2_GS_REG_CSR = EF2_GS_CSR_RESET;
    ef2_gs_sync();
    *EF2_GS_REG_CSR = 0;
    ef2_gs_sync();

    ef2_kernel_set_gs_crt(
        (ef2_s16)(config->interlaced ? 1 : 0),
        (ef2_s16)config->standard,
        (ef2_s16)config->field_mode);

    ef2_video_set_background(0, 0, 0);
    ef2_video_output_background();

    return 0;
}
