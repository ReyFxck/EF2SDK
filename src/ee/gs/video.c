#include <ef2/gif.h>
#include <ef2/gs.h>
#include <ef2/kernel.h>
#include <ef2/sif.h>
#include <ef2/video.h>

#define EF2_GS_TEXTURE_PAGE_BYTES 8192u
#define EF2_GS_TEXTURE_PAGE_WIDTH 64u
#define EF2_GS_TEXTURE_PAGE_HEIGHT 32u
#define EF2_GIF_IMAGE_MAX_QWORDS 0x7FFFu

static ef2_u16 ef2_video_width;
static ef2_u16 ef2_video_height;
static ef2_video_config ef2_video_active_config;
static ef2_video_framebuffer_layout ef2_video_active_layout;
static ef2_u32 ef2_video_gif_dma_available;
static ef2_u32 ef2_video_gif_dma_fallbacks;
static ef2_u32 ef2_video_texture_start;
static ef2_u32 ef2_video_texture_cursor;
static ef2_u32 ef2_video_framebuffer_bytes;
static ef2_u32 ef2_video_framebuffer_base[2];
static ef2_u32 ef2_video_display_buffer;
static ef2_u32 ef2_video_draw_buffer;
static ef2_u32 ef2_video_double_buffered;
static ef2_u32 ef2_video_present_count;
static ef2_u32 ef2_video_vsync_timeouts;
static ef2_profile_counter ef2_video_vsync_wait_ticks;
static ef2_u32 ef2_video_clut_stage[256] EF2_ALIGN(16);

static int ef2_video_submit_qwords(
    const ef2_gif_qword *packet,
    ef2_u32 count);

static ef2_u16 ef2_video_frame_fbp(ef2_u32 index)
{
    return (ef2_u16)(
        ef2_video_framebuffer_base[index & 1u] /
        8192u);
}

static ef2_u16 ef2_video_draw_fbp(void)
{
    return ef2_video_frame_fbp(
        ef2_video_draw_buffer);
}

static void ef2_video_program_display_buffer(
    ef2_u32 index)
{
    *EF2_GS_REG_DISPFB2 =
        ef2_gs_pack_dispfb(
            ef2_video_frame_fbp(index),
            ef2_video_active_layout.buffer_width,
            ef2_video_active_layout.gs_psm,
            0,
            0);

    ef2_gs_sync();
}

static int ef2_video_standard_valid(
    ef2_video_standard standard)
{
    return standard == EF2_VIDEO_NTSC ||
           standard == EF2_VIDEO_PAL;
}

static int ef2_video_framebuffer_format_info(
    ef2_video_framebuffer_format format,
    ef2_u8 *psm,
    ef2_u16 *page_height)
{
    if (psm == (ef2_u8 *)0 ||
        page_height == (ef2_u16 *)0)
        return -1;

    if (format == EF2_VIDEO_FB_RGBA32) {
        *psm = EF2_GS_PSMCT32;
        *page_height = 32u;
        return 0;
    }

    if (format == EF2_VIDEO_FB_RGB16) {
        *psm = EF2_GS_PSMCT16;
        *page_height = 64u;
        return 0;
    }

    return -2;
}

int ef2_video_detect_standard(
    ef2_video_standard *standard)
{
    char romver[15];
    int result;

    if (standard == (ef2_video_standard *)0)
        return -1;

    result = ef2_iop_get_romver(
        romver,
        sizeof(romver));
    if (result < 0)
        return -2;

    *standard =
        romver[4] == 'E'
            ? EF2_VIDEO_PAL
            : EF2_VIDEO_NTSC;

    return 0;
}

static void ef2_gif_ad(
    ef2_gif_qword *qword,
    ef2_u64 data,
    ef2_u64 address)
{
    qword->lo = data;
    qword->hi = address;
}

static ef2_u32 ef2_align_up_u32(
    ef2_u32 value,
    ef2_u32 alignment)
{
    return (value + alignment - 1u) &
           ~(alignment - 1u);
}

static ef2_u8 ef2_log2_ceil_u16(ef2_u16 value)
{
    ef2_u32 power = 1u;
    ef2_u8 exponent = 0;

    while (power < (ef2_u32)value &&
           exponent < 15u) {
        power <<= 1;
        ++exponent;
    }

    return exponent;
}

static ef2_s32 ef2_clamp_s32(
    ef2_s32 value,
    ef2_s32 low,
    ef2_s32 high)
{
    if (value < low)
        return low;
    if (value > high)
        return high;
    return value;
}

static int ef2_video_upload_image(
    ef2_u16 destination,
    ef2_u8 destination_width,
    ef2_u8 psm,
    ef2_u16 width,
    ef2_u16 height,
    const void *pixels,
    ef2_u32 transfer_bytes)
{
    ef2_gif_qword setup[5] EF2_ALIGN(16);
    ef2_gif_qword image_tag EF2_ALIGN(16);
    ef2_u32 transfer_qwords;
    ef2_u32 remaining;
    ef2_u32 offset_qwords = 0;

    if (pixels == (const void *)0 ||
        transfer_bytes == 0u ||
        (transfer_bytes & 0x0Fu) != 0u)
        return -1;

    setup[0].lo =
        ef2_gif_pack_tag(
            4, 1, 0, 0,
            EF2_GIF_FLG_PACKED, 1);
    setup[0].hi = EF2_GIF_REG_AD;

    ef2_gif_ad(
        &setup[1],
        ef2_gs_pack_bitbltbuf(
            destination,
            destination_width,
            psm),
        EF2_GS_ADDR_BITBLTBUF);

    ef2_gif_ad(
        &setup[2],
        ef2_gs_pack_trxpos(0, 0),
        EF2_GS_ADDR_TRXPOS);

    ef2_gif_ad(
        &setup[3],
        ef2_gs_pack_trxreg(width, height),
        EF2_GS_ADDR_TRXREG);

    ef2_gif_ad(
        &setup[4],
        0,
        EF2_GS_ADDR_TRXDIR);

    if (ef2_video_submit_qwords(setup, 5) < 0)
        return -2;

    transfer_qwords = transfer_bytes >> 4;
    remaining = transfer_qwords;

    while (remaining != 0u) {
        ef2_u32 chunk =
            remaining > EF2_GIF_IMAGE_MAX_QWORDS
                ? EF2_GIF_IMAGE_MAX_QWORDS
                : remaining;

        image_tag.lo =
            ef2_gif_pack_tag(
                (ef2_u16)chunk,
                1,
                0,
                0,
                EF2_GIF_FLG_IMAGE,
                0);
        image_tag.hi = 0;

        if (ef2_video_submit_qwords(
                &image_tag, 1) < 0)
            return -3;

        if (ef2_video_submit_qwords(
                (const ef2_gif_qword *)pixels +
                    offset_qwords,
                chunk) < 0)
            return -4;

        offset_qwords += chunk;
        remaining -= chunk;
    }

    return 0;
}

static int ef2_video_flush_texture_cache(void)
{
    ef2_gif_qword flush[2] EF2_ALIGN(16);

    flush[0].lo =
        ef2_gif_pack_tag(
            1, 1, 0, 0,
            EF2_GIF_FLG_PACKED, 1);
    flush[0].hi = EF2_GIF_REG_AD;

    ef2_gif_ad(
        &flush[1],
        0,
        EF2_GS_ADDR_TEXFLUSH);

    return ef2_video_submit_qwords(flush, 2);
}

static void ef2_video_prepare_csm1_clut(
    const ef2_u32 *palette)
{
    ef2_u32 i;

    for (i = 0; i < 256u; ++i) {
        ef2_u32 destination =
            (i & ~0x18u) |
            ((i & 0x08u) << 1) |
            ((i & 0x10u) >> 1);

        ef2_video_clut_stage[destination] =
            palette[i];
    }
}

static int ef2_video_submit_qwords(
    const ef2_gif_qword *packet,
    ef2_u32 count)
{
    if (ef2_video_gif_dma_available) {
        if (ef2_gif_dma_send_qwords(
                packet,
                count) == 0)
            return 0;

        ++ef2_video_gif_dma_fallbacks;
        ef2_video_gif_dma_available = 0;
    }

    ef2_gif_send_qwords(packet, count);
    return 0;
}

static int ef2_video_draw_two_vertex(
    ef2_u8 primitive,
    ef2_s32 x0,
    ef2_s32 y0,
    ef2_s32 x1,
    ef2_s32 y1,
    ef2_u8 r,
    ef2_u8 g,
    ef2_u8 b)
{
    ef2_gif_qword packet[9] EF2_ALIGN(16);
    ef2_u16 fx0;
    ef2_u16 fy0;
    ef2_u16 fx1;
    ef2_u16 fy1;

    if (ef2_video_width == 0u ||
        ef2_video_height == 0u)
        return -1;

    x0 = ef2_clamp_s32(
        x0, 0, (ef2_s32)ef2_video_width);
    y0 = ef2_clamp_s32(
        y0, 0, (ef2_s32)ef2_video_height);
    x1 = ef2_clamp_s32(
        x1, 0, (ef2_s32)ef2_video_width);
    y1 = ef2_clamp_s32(
        y1, 0, (ef2_s32)ef2_video_height);

    fx0 = (ef2_u16)((ef2_u32)x0 << 4);
    fy0 = (ef2_u16)((ef2_u32)y0 << 4);
    fx1 = (ef2_u16)((ef2_u32)x1 << 4);
    fy1 = (ef2_u16)((ef2_u32)y1 << 4);

    packet[0].lo =
        ef2_gif_pack_tag(
            8, 1, 0, 0,
            EF2_GIF_FLG_PACKED, 1);
    packet[0].hi = EF2_GIF_REG_AD;

    ef2_gif_ad(
        &packet[1],
        ef2_gs_pack_frame(
            ef2_video_draw_fbp(),
            ef2_video_active_layout.buffer_width,
            ef2_video_active_layout.gs_psm,
            0),
        EF2_GS_ADDR_FRAME_1);

    ef2_gif_ad(
        &packet[2],
        ef2_gs_pack_xyoffset(0, 0),
        EF2_GS_ADDR_XYOFFSET_1);

    ef2_gif_ad(
        &packet[3],
        ef2_gs_pack_scissor(
            0,
            (ef2_u16)(ef2_video_width - 1u),
            0,
            (ef2_u16)(ef2_video_height - 1u)),
        EF2_GS_ADDR_SCISSOR_1);

    ef2_gif_ad(
        &packet[4],
        0,
        EF2_GS_ADDR_TEST_1);

    ef2_gif_ad(
        &packet[5],
        ef2_gs_pack_prim(primitive),
        EF2_GS_ADDR_PRIM);

    ef2_gif_ad(
        &packet[6],
        ef2_gs_pack_rgbaq(r, g, b, 0x80),
        EF2_GS_ADDR_RGBAQ);

    ef2_gif_ad(
        &packet[7],
        ef2_gs_pack_xyz(fx0, fy0, 0),
        EF2_GS_ADDR_XYZ2);

    ef2_gif_ad(
        &packet[8],
        ef2_gs_pack_xyz(fx1, fy1, 0),
        EF2_GS_ADDR_XYZ2);

    return ef2_video_submit_qwords(packet, 9);
}

void ef2_video_set_background(
    ef2_u8 r,
    ef2_u8 g,
    ef2_u8 b)
{
    *EF2_GS_REG_BGCOLOR =
        ef2_gs_pack_bgcolor(r, g, b);
    ef2_gs_sync();
}

int ef2_video_get_config(
    ef2_video_config *config)
{
    if (config == (ef2_video_config *)0 ||
        ef2_video_width == 0u ||
        ef2_video_height == 0u)
        return -1;

    *config = ef2_video_active_config;
    return 0;
}

int ef2_video_get_framebuffer_layout(
    ef2_video_framebuffer_layout *layout)
{
    if (layout ==
            (ef2_video_framebuffer_layout *)0 ||
        ef2_video_width == 0u ||
        ef2_video_height == 0u)
        return -1;

    *layout = ef2_video_active_layout;
    return 0;
}

int ef2_video_get_size(
    ef2_u16 *width,
    ef2_u16 *height)
{
    if (width == (ef2_u16 *)0 ||
        height == (ef2_u16 *)0 ||
        ef2_video_width == 0u ||
        ef2_video_height == 0u)
        return -1;

    *width = ef2_video_width;
    *height = ef2_video_height;
    return 0;
}

int ef2_video_get_transport_stats(
    ef2_video_transport_stats *stats)
{
    if (stats ==
        (ef2_video_transport_stats *)0)
        return -1;

    stats->dma_available =
        ef2_video_gif_dma_available;
    stats->dma_fallbacks =
        ef2_video_gif_dma_fallbacks;

    return 0;
}

int ef2_video_get_frame_stats(
    ef2_video_frame_stats *stats)
{
    if (stats == (ef2_video_frame_stats *)0)
        return -1;

    stats->double_buffered =
        ef2_video_double_buffered;
    stats->display_buffer =
        ef2_video_display_buffer;
    stats->draw_buffer =
        ef2_video_draw_buffer;
    stats->presents =
        ef2_video_present_count;
    stats->vsync_timeouts =
        ef2_video_vsync_timeouts;
    stats->vsync_wait_ticks =
        ef2_video_vsync_wait_ticks;

    return 0;
}

int ef2_video_set_double_buffering(ef2_u32 enabled)
{
    if (ef2_video_width == 0u ||
        ef2_video_height == 0u)
        return -1;

    if (enabled != 0u) {
        if (!ef2_video_double_buffered) {
            ef2_video_double_buffered = 1u;
            ef2_video_draw_buffer =
                1u - ef2_video_display_buffer;
        }
    } else {
        ef2_video_double_buffered = 0u;
        ef2_video_draw_buffer =
            ef2_video_display_buffer;
    }

    return 0;
}

int ef2_video_wait_vsync(ef2_u32 timeout)
{
    ef2_u32 profile_start;

    if (ef2_video_width == 0u ||
        ef2_video_height == 0u)
        return -1;

    profile_start = ef2_profile_begin();

    if (timeout == 0u)
        timeout = 0x04000000u;

    /*
     * VSINT is write-one-to-clear. Clear a stale event first, then wait
     * for the next vertical sync edge. This uses the GS CSR directly and
     * does not require an EE interrupt handler.
     */
    *EF2_GS_REG_CSR = EF2_GS_CSR_VSINT;
    ef2_gs_sync();

    while (((*EF2_GS_REG_CSR) &
            EF2_GS_CSR_VSINT) == 0u) {
        if (--timeout == 0u) {
            (void)ef2_profile_end(
                &ef2_video_vsync_wait_ticks,
                profile_start);
            ++ef2_video_vsync_timeouts;
            return -2;
        }

        __asm__ volatile("nop");
    }

    (void)ef2_profile_end(
        &ef2_video_vsync_wait_ticks,
        profile_start);
    return 0;
}

int ef2_video_present(void)
{
    ef2_u32 old_display;
    int result;

    if (ef2_video_width == 0u ||
        ef2_video_height == 0u)
        return -1;

    result = ef2_video_wait_vsync(0);
    if (result < 0)
        return result;

    if (ef2_video_double_buffered) {
        old_display = ef2_video_display_buffer;
        ef2_video_display_buffer =
            ef2_video_draw_buffer;

        ef2_video_program_display_buffer(
            ef2_video_display_buffer);

        ef2_video_draw_buffer = old_display;
    }

    ++ef2_video_present_count;
    return 0;
}

int ef2_video_draw_rect(
    ef2_s32 x,
    ef2_s32 y,
    ef2_s32 width,
    ef2_s32 height,
    ef2_u8 r,
    ef2_u8 g,
    ef2_u8 b)
{
    ef2_s32 right;
    ef2_s32 bottom;

    if (width <= 0 || height <= 0)
        return -1;

    if (x >= (ef2_s32)ef2_video_width ||
        y >= (ef2_s32)ef2_video_height ||
        x + width <= 0 ||
        y + height <= 0)
        return 0;

    right = x + width;
    bottom = y + height;

    x = ef2_clamp_s32(
        x, 0, (ef2_s32)ef2_video_width);
    y = ef2_clamp_s32(
        y, 0, (ef2_s32)ef2_video_height);
    right = ef2_clamp_s32(
        right, 0, (ef2_s32)ef2_video_width);
    bottom = ef2_clamp_s32(
        bottom, 0, (ef2_s32)ef2_video_height);

    if (right <= x || bottom <= y)
        return 0;

    return ef2_video_draw_two_vertex(
        EF2_GS_PRIM_SPRITE,
        x, y, right, bottom,
        r, g, b);
}

int ef2_video_draw_line(
    ef2_s32 x0,
    ef2_s32 y0,
    ef2_s32 x1,
    ef2_s32 y1,
    ef2_u8 r,
    ef2_u8 g,
    ef2_u8 b)
{
    if (ef2_video_width == 0u ||
        ef2_video_height == 0u)
        return -1;

    return ef2_video_draw_two_vertex(
        EF2_GS_PRIM_LINE,
        x0, y0, x1, y1,
        r, g, b);
}

int ef2_video_clear(
    ef2_u8 r,
    ef2_u8 g,
    ef2_u8 b)
{
    if (ef2_video_width == 0u ||
        ef2_video_height == 0u)
        return -1;

    return ef2_video_draw_rect(
        0, 0,
        ef2_video_width,
        ef2_video_height,
        r, g, b);
}

void ef2_video_reset_texture_allocator(void)
{
    ef2_video_texture_cursor =
        ef2_video_texture_start;
}

ef2_u32 ef2_video_get_texture_vram_free(void)
{
    (void)ef2_video_framebuffer_bytes;
    if (ef2_video_texture_cursor >=
        EF2_GS_VRAM_BYTES)
        return 0;

    return EF2_GS_VRAM_BYTES -
           ef2_video_texture_cursor;
}

int ef2_video_upload_rgba32(
    ef2_video_texture *texture,
    const ef2_u32 *pixels,
    ef2_u16 width,
    ef2_u16 height)
{
    ef2_u32 padded_width;
    ef2_u32 padded_height;
    ef2_u32 allocation_bytes;
    ef2_u32 transfer_bytes;
    ef2_u32 cursor;
    ef2_u8 tbw;

    if (texture == (ef2_video_texture *)0 ||
        pixels == (const ef2_u32 *)0 ||
        width == 0u || height == 0u ||
        width > 1024u || height > 1024u)
        return -1;

    if (((ef2_u32)pixels & 0x0Fu) != 0u)
        return -2;

    transfer_bytes =
        (ef2_u32)width *
        (ef2_u32)height * 4u;

    if ((transfer_bytes & 0x0Fu) != 0u)
        return -3;

    padded_width = ef2_align_up_u32(
        width,
        EF2_GS_TEXTURE_PAGE_WIDTH);
    padded_height = ef2_align_up_u32(
        height,
        EF2_GS_TEXTURE_PAGE_HEIGHT);

    allocation_bytes =
        padded_width *
        padded_height * 4u;

    cursor = ef2_align_up_u32(
        ef2_video_texture_cursor,
        EF2_GS_TEXTURE_PAGE_BYTES);

    if (cursor > EF2_GS_VRAM_BYTES ||
        allocation_bytes >
            EF2_GS_VRAM_BYTES - cursor)
        return -4;

    tbw = (ef2_u8)(padded_width / 64u);

    texture->vram_address =
        (ef2_u16)(cursor / 256u);
    texture->clut_address = 0;
    texture->width = width;
    texture->height = height;
    texture->buffer_width = tbw;
    texture->psm = EF2_GS_PSMCT32;
    texture->clut_psm = 0;
    texture->width_log2 =
        ef2_log2_ceil_u16(width);
    texture->height_log2 =
        ef2_log2_ceil_u16(height);
    texture->valid = 0;
    texture->indexed = 0;
    texture->reserved[0] = 0;
    texture->reserved[1] = 0;

    if (ef2_video_upload_image(
            texture->vram_address,
            texture->buffer_width,
            EF2_GS_PSMCT32,
            width,
            height,
            pixels,
            transfer_bytes) < 0)
        return -5;

    if (ef2_video_flush_texture_cache() < 0)
        return -6;

    texture->valid = 1;
    ef2_video_texture_cursor =
        cursor + allocation_bytes;

    return 0;
}

int ef2_video_upload_indexed8(
    ef2_video_texture *texture,
    const ef2_u8 *indices,
    const ef2_u32 *palette_rgba32,
    ef2_u16 width,
    ef2_u16 height)
{
    ef2_u32 padded_width;
    ef2_u32 padded_height;
    ef2_u32 texture_bytes;
    ef2_u32 index_bytes;
    ef2_u32 texture_cursor;
    ef2_u32 clut_cursor;
    ef2_u32 end_cursor;
    ef2_u8 tbw;

    if (texture == (ef2_video_texture *)0 ||
        indices == (const ef2_u8 *)0 ||
        palette_rgba32 == (const ef2_u32 *)0 ||
        width == 0u || height == 0u ||
        width > 1024u || height > 1024u)
        return -1;

    if (((ef2_u32)indices & 0x0Fu) != 0u ||
        ((ef2_u32)palette_rgba32 & 0x0Fu) != 0u)
        return -2;

    index_bytes =
        (ef2_u32)width *
        (ef2_u32)height;

    if ((index_bytes & 0x0Fu) != 0u)
        return -3;

    padded_width =
        ef2_align_up_u32(width, 128u);
    padded_height =
        ef2_align_up_u32(height, 64u);

    texture_bytes =
        padded_width * padded_height;

    texture_cursor =
        ef2_align_up_u32(
            ef2_video_texture_cursor,
            EF2_GS_TEXTURE_PAGE_BYTES);

    clut_cursor =
        ef2_align_up_u32(
            texture_cursor + texture_bytes,
            256u);

    end_cursor = clut_cursor + 1024u;

    if (end_cursor > EF2_GS_VRAM_BYTES)
        return -4;

    tbw = (ef2_u8)(padded_width / 64u);

    texture->vram_address =
        (ef2_u16)(texture_cursor / 256u);
    texture->clut_address =
        (ef2_u16)(clut_cursor / 256u);
    texture->width = width;
    texture->height = height;
    texture->buffer_width = tbw;
    texture->psm = EF2_GS_PSMT8;
    texture->clut_psm = EF2_GS_PSMCT32;
    texture->width_log2 =
        ef2_log2_ceil_u16(width);
    texture->height_log2 =
        ef2_log2_ceil_u16(height);
    texture->valid = 0;
    texture->indexed = 1;
    texture->reserved[0] = 0;
    texture->reserved[1] = 0;

    if (ef2_video_upload_image(
            texture->vram_address,
            texture->buffer_width,
            EF2_GS_PSMT8,
            width,
            height,
            indices,
            index_bytes) < 0)
        return -5;

    ef2_video_prepare_csm1_clut(
        palette_rgba32);

    /*
     * CSM1 RGBA32 CLUT is uploaded as a 16x16 CT32 image.
     * DBW remains one 64-pixel unit just like the established draw path.
     */
    if (ef2_video_upload_image(
            texture->clut_address,
            1,
            EF2_GS_PSMCT32,
            16,
            16,
            ef2_video_clut_stage,
            1024u) < 0)
        return -6;

    if (ef2_video_flush_texture_cache() < 0)
        return -7;

    texture->valid = 1;
    ef2_video_texture_cursor = end_cursor;
    return 0;
}

int ef2_video_pack_indices4(
    ef2_u8 *packed,
    ef2_u32 packed_size,
    const ef2_u8 *indices,
    ef2_u32 pixel_count)
{
    ef2_u32 required;
    ef2_u32 pair;

    if (packed == (ef2_u8 *)0 ||
        indices == (const ef2_u8 *)0)
        return -1;

    required = (pixel_count + 1u) >> 1;

    if (packed_size < required)
        return -2;

    for (pair = 0; pair < (pixel_count >> 1); ++pair) {
        ef2_u8 low =
            indices[pair * 2u] & 0x0Fu;
        ef2_u8 high =
            indices[pair * 2u + 1u] & 0x0Fu;

        packed[pair] =
            (ef2_u8)(low | (high << 4));
    }

    if ((pixel_count & 1u) != 0u)
        packed[pixel_count >> 1] =
            indices[pixel_count - 1u] & 0x0Fu;

    return (int)required;
}

int ef2_video_upload_indexed4(
    ef2_video_texture *texture,
    const ef2_u8 *packed_indices,
    const ef2_u32 *palette_rgba32_16,
    ef2_u16 width,
    ef2_u16 height)
{
    ef2_u32 padded_width;
    ef2_u32 padded_height;
    ef2_u32 texture_bytes;
    ef2_u32 pixel_count;
    ef2_u32 packed_bytes;
    ef2_u32 texture_cursor;
    ef2_u32 clut_cursor;
    ef2_u32 end_cursor;
    ef2_u8 tbw;

    if (texture == (ef2_video_texture *)0 ||
        packed_indices == (const ef2_u8 *)0 ||
        palette_rgba32_16 == (const ef2_u32 *)0 ||
        width == 0u || height == 0u ||
        width > 1024u || height > 1024u)
        return -1;

    if (((ef2_u32)packed_indices & 0x0Fu) != 0u ||
        ((ef2_u32)palette_rgba32_16 & 0x0Fu) != 0u)
        return -2;

    pixel_count =
        (ef2_u32)width *
        (ef2_u32)height;
    packed_bytes =
        (pixel_count + 1u) >> 1;

    if ((packed_bytes & 0x0Fu) != 0u)
        return -3;

    padded_width =
        ef2_align_up_u32(width, 128u);
    padded_height =
        ef2_align_up_u32(height, 128u);

    texture_bytes =
        (padded_width * padded_height) >> 1;

    texture_cursor =
        ef2_align_up_u32(
            ef2_video_texture_cursor,
            EF2_GS_TEXTURE_PAGE_BYTES);

    clut_cursor =
        ef2_align_up_u32(
            texture_cursor + texture_bytes,
            256u);

    end_cursor = clut_cursor + 256u;

    if (end_cursor > EF2_GS_VRAM_BYTES)
        return -4;

    tbw = (ef2_u8)(padded_width / 64u);

    texture->vram_address =
        (ef2_u16)(texture_cursor / 256u);
    texture->clut_address =
        (ef2_u16)(clut_cursor / 256u);
    texture->width = width;
    texture->height = height;
    texture->buffer_width = tbw;
    texture->psm = EF2_GS_PSMT4;
    texture->clut_psm = EF2_GS_PSMCT32;
    texture->width_log2 =
        ef2_log2_ceil_u16(width);
    texture->height_log2 =
        ef2_log2_ceil_u16(height);
    texture->valid = 0;
    texture->indexed = 1;
    texture->reserved[0] = 0;
    texture->reserved[1] = 0;

    if (ef2_video_upload_image(
            texture->vram_address,
            texture->buffer_width,
            EF2_GS_PSMT4,
            width,
            height,
            packed_indices,
            packed_bytes) < 0)
        return -5;

    if (ef2_video_upload_image(
            texture->clut_address,
            1,
            EF2_GS_PSMCT32,
            8,
            2,
            palette_rgba32_16,
            64u) < 0)
        return -6;

    if (ef2_video_flush_texture_cache() < 0)
        return -7;

    texture->valid = 1;
    ef2_video_texture_cursor = end_cursor;
    return 0;
}


int ef2_video_draw_texture_region(
    const ef2_video_texture *texture,
    ef2_u16 source_x,
    ef2_u16 source_y,
    ef2_u16 source_width,
    ef2_u16 source_height,
    ef2_s32 x,
    ef2_s32 y,
    ef2_s32 width,
    ef2_s32 height,
    ef2_u8 r,
    ef2_u8 g,
    ef2_u8 b,
    ef2_u8 a,
    ef2_u8 blend)
{
    ef2_gif_qword packet[15] EF2_ALIGN(16);
    ef2_s32 right;
    ef2_s32 bottom;
    ef2_u16 u0;
    ef2_u16 v0;
    ef2_u16 u1;
    ef2_u16 v1;

    if (texture ==
            (const ef2_video_texture *)0 ||
        !texture->valid ||
        (texture->psm != EF2_GS_PSMCT32 &&
         texture->psm != EF2_GS_PSMT8 &&
         texture->psm != EF2_GS_PSMT4) ||
        source_width == 0u ||
        source_height == 0u ||
        width <= 0 || height <= 0)
        return -1;

    if ((ef2_u32)source_x +
            (ef2_u32)source_width >
            texture->width ||
        (ef2_u32)source_y +
            (ef2_u32)source_height >
            texture->height)
        return -2;

    if (x < 0 || y < 0 ||
        x + width > (ef2_s32)ef2_video_width ||
        y + height > (ef2_s32)ef2_video_height)
        return -3;

    right = x + width;
    bottom = y + height;

    u0 = (ef2_u16)(source_x << 4);
    v0 = (ef2_u16)(source_y << 4);
    u1 = (ef2_u16)(
        ((ef2_u32)source_x +
         source_width) << 4);
    v1 = (ef2_u16)(
        ((ef2_u32)source_y +
         source_height) << 4);

    packet[0].lo =
        ef2_gif_pack_tag(
            14, 1, 0, 0,
            EF2_GIF_FLG_PACKED, 1);
    packet[0].hi = EF2_GIF_REG_AD;

    ef2_gif_ad(
        &packet[1],
        ef2_gs_pack_frame(
            ef2_video_draw_fbp(),
            ef2_video_active_layout.buffer_width,
            ef2_video_active_layout.gs_psm,
            0),
        EF2_GS_ADDR_FRAME_1);

    ef2_gif_ad(
        &packet[2],
        ef2_gs_pack_xyoffset(0, 0),
        EF2_GS_ADDR_XYOFFSET_1);

    ef2_gif_ad(
        &packet[3],
        ef2_gs_pack_scissor(
            0,
            (ef2_u16)(ef2_video_width - 1u),
            0,
            (ef2_u16)(ef2_video_height - 1u)),
        EF2_GS_ADDR_SCISSOR_1);

    ef2_gif_ad(
        &packet[4],
        0,
        EF2_GS_ADDR_TEST_1);

    if (texture->indexed) {
        ef2_gif_ad(
            &packet[5],
            ef2_gs_pack_tex0_clut(
                texture->vram_address,
                texture->buffer_width,
                texture->psm,
                texture->width_log2,
                texture->height_log2,
                1,
                0,
                texture->clut_address,
                texture->clut_psm,
                0,
                0,
                1),
            EF2_GS_ADDR_TEX0_1);
    } else {
        ef2_gif_ad(
            &packet[5],
            ef2_gs_pack_tex0(
                texture->vram_address,
                texture->buffer_width,
                texture->psm,
                texture->width_log2,
                texture->height_log2,
                1,
                0),
            EF2_GS_ADDR_TEX0_1);
    }

    ef2_gif_ad(
        &packet[6],
        0,
        EF2_GS_ADDR_TEX1_1);

    ef2_gif_ad(
        &packet[7],
        1,
        EF2_GS_ADDR_PRMODECONT);

    /*
     * Standard source-over:
     * (Cs - Cd) * As / 128 + Cd.
     * A=source, B=dest, C=source alpha, D=dest.
     */
    ef2_gif_ad(
        &packet[8],
        ef2_gs_pack_alpha(
            0, 1, 0, 1, 0x80),
        EF2_GS_ADDR_ALPHA_1);

    ef2_gif_ad(
        &packet[9],
        ef2_gs_pack_prim_ex(
            EF2_GS_PRIM_SPRITE,
            0, 1, 0,
            blend ? 1u : 0u,
            0, 1, 0, 0),
        EF2_GS_ADDR_PRIM);

    /*
     * TFX=MODULATE uses 0x80 as unity. This gives us a tint and a
     * caller-controlled alpha without changing texture memory.
     */
    ef2_gif_ad(
        &packet[10],
        ef2_gs_pack_rgbaq(
            r, g, b, a),
        EF2_GS_ADDR_RGBAQ);

    ef2_gif_ad(
        &packet[11],
        ef2_gs_pack_uv(u0, v0),
        EF2_GS_ADDR_UV);

    ef2_gif_ad(
        &packet[12],
        ef2_gs_pack_xyz(
            (ef2_u16)((ef2_u32)x << 4),
            (ef2_u16)((ef2_u32)y << 4),
            0),
        EF2_GS_ADDR_XYZ2);

    ef2_gif_ad(
        &packet[13],
        ef2_gs_pack_uv(u1, v1),
        EF2_GS_ADDR_UV);

    ef2_gif_ad(
        &packet[14],
        ef2_gs_pack_xyz(
            (ef2_u16)((ef2_u32)right << 4),
            (ef2_u16)((ef2_u32)bottom << 4),
            0),
        EF2_GS_ADDR_XYZ2);

    return ef2_video_submit_qwords(packet, 15);
}

int ef2_video_draw_texture(
    const ef2_video_texture *texture,
    ef2_s32 x,
    ef2_s32 y,
    ef2_s32 width,
    ef2_s32 height)
{
    ef2_gif_qword packet[14] EF2_ALIGN(16);
    ef2_s32 right;
    ef2_s32 bottom;
    ef2_u16 u1;
    ef2_u16 v1;

    if (texture ==
            (const ef2_video_texture *)0 ||
        !texture->valid ||
        (texture->psm != EF2_GS_PSMCT32 &&
         texture->psm != EF2_GS_PSMT8 &&
         texture->psm != EF2_GS_PSMT4) ||
        width <= 0 || height <= 0)
        return -1;

    if (x < 0 || y < 0 ||
        x + width > (ef2_s32)ef2_video_width ||
        y + height > (ef2_s32)ef2_video_height)
        return -2;

    right = x + width;
    bottom = y + height;
    u1 = (ef2_u16)(texture->width << 4);
    v1 = (ef2_u16)(texture->height << 4);

    packet[0].lo =
        ef2_gif_pack_tag(
            13, 1, 0, 0,
            EF2_GIF_FLG_PACKED, 1);
    packet[0].hi = EF2_GIF_REG_AD;

    ef2_gif_ad(
        &packet[1],
        ef2_gs_pack_frame(
            ef2_video_draw_fbp(),
            ef2_video_active_layout.buffer_width,
            ef2_video_active_layout.gs_psm,
            0),
        EF2_GS_ADDR_FRAME_1);

    ef2_gif_ad(
        &packet[2],
        ef2_gs_pack_xyoffset(0, 0),
        EF2_GS_ADDR_XYOFFSET_1);

    ef2_gif_ad(
        &packet[3],
        ef2_gs_pack_scissor(
            0,
            (ef2_u16)(ef2_video_width - 1u),
            0,
            (ef2_u16)(ef2_video_height - 1u)),
        EF2_GS_ADDR_SCISSOR_1);

    ef2_gif_ad(
        &packet[4],
        0,
        EF2_GS_ADDR_TEST_1);

    if (texture->indexed) {
        ef2_gif_ad(
            &packet[5],
            ef2_gs_pack_tex0_clut(
                texture->vram_address,
                texture->buffer_width,
                texture->psm,
                texture->width_log2,
                texture->height_log2,
                1,
                1,
                texture->clut_address,
                texture->clut_psm,
                0,
                0,
                1),
            EF2_GS_ADDR_TEX0_1);
    } else {
        ef2_gif_ad(
            &packet[5],
            ef2_gs_pack_tex0(
                texture->vram_address,
                texture->buffer_width,
                texture->psm,
                texture->width_log2,
                texture->height_log2,
                1,
                1),
            EF2_GS_ADDR_TEX0_1);
    }

    ef2_gif_ad(
        &packet[6],
        0,
        EF2_GS_ADDR_TEX1_1);

    /*
     * This is deliberately the alpha.25 validated state:
     * PRIM owns TME/FST and TEX0 uses DECAL.
     */
    ef2_gif_ad(
        &packet[7],
        1,
        EF2_GS_ADDR_PRMODECONT);

    ef2_gif_ad(
        &packet[8],
        ef2_gs_pack_prim_ex(
            EF2_GS_PRIM_SPRITE,
            0, 1, 0, 0, 0, 1, 0, 0),
        EF2_GS_ADDR_PRIM);

    ef2_gif_ad(
        &packet[9],
        ef2_gs_pack_rgbaq(
            0x80, 0x80, 0x80, 0x80),
        EF2_GS_ADDR_RGBAQ);

    ef2_gif_ad(
        &packet[10],
        ef2_gs_pack_uv(0, 0),
        EF2_GS_ADDR_UV);

    ef2_gif_ad(
        &packet[11],
        ef2_gs_pack_xyz(
            (ef2_u16)((ef2_u32)x << 4),
            (ef2_u16)((ef2_u32)y << 4),
            0),
        EF2_GS_ADDR_XYZ2);

    ef2_gif_ad(
        &packet[12],
        ef2_gs_pack_uv(u1, v1),
        EF2_GS_ADDR_UV);

    ef2_gif_ad(
        &packet[13],
        ef2_gs_pack_xyz(
            (ef2_u16)((ef2_u32)right << 4),
            (ef2_u16)((ef2_u32)bottom << 4),
            0),
        EF2_GS_ADDR_XYZ2);

    return ef2_video_submit_qwords(packet, 14);
}

int ef2_video_init(
    const ef2_video_config *config)
{
    ef2_video_standard standard;
    ef2_video_framebuffer_format format;
    ef2_u16 default_height;
    ef2_u16 page_height;
    ef2_u16 dx;
    ef2_u16 dy;
    ef2_u32 horizontal_scale;
    ef2_u32 vertical_scale;
    ef2_u32 page_columns;
    ef2_u32 page_rows;
    ef2_u32 framebuffer_bytes;
    ef2_u8 framebuffer_psm;

    if (config ==
        (const ef2_video_config *)0)
        return -1;

    standard = config->standard;
    if (standard == EF2_VIDEO_AUTO) {
        if (ef2_video_detect_standard(
                &standard) < 0)
            standard = EF2_VIDEO_NTSC;
    }

    if (!ef2_video_standard_valid(standard))
        return -2;

    if (!config->interlaced ||
        config->field_mode != EF2_VIDEO_FIELD)
        return -3;

    format = config->framebuffer_format;
    if (ef2_video_framebuffer_format_info(
            format,
            &framebuffer_psm,
            &page_height) < 0)
        return -4;

    if (standard == EF2_VIDEO_PAL) {
        default_height = 512u;
        dx = 680;
        dy = 72;
    } else {
        default_height = 448u;
        dx = 656;
        dy = 36;
    }

    ef2_video_width =
        config->framebuffer_width != 0u
            ? config->framebuffer_width
            : 640u;
    ef2_video_height =
        config->framebuffer_height != 0u
            ? config->framebuffer_height
            : default_height;

    if (ef2_video_width < 160u ||
        ef2_video_width > 1024u ||
        (ef2_video_width & 63u) != 0u ||
        ef2_video_height == 0u ||
        ef2_video_height > default_height ||
        (2560u % ef2_video_width) != 0u ||
        ((ef2_u32)default_height %
         ef2_video_height) != 0u)
        return -5;

    horizontal_scale =
        2560u / ef2_video_width;
    vertical_scale =
        (ef2_u32)default_height /
        ef2_video_height;

    if (horizontal_scale == 0u ||
        horizontal_scale > 16u ||
        vertical_scale == 0u ||
        vertical_scale > 4u)
        return -5;

    page_columns =
        ((ef2_u32)ef2_video_width + 63u) /
        64u;
    page_rows =
        ((ef2_u32)ef2_video_height +
         (ef2_u32)page_height - 1u) /
        (ef2_u32)page_height;
    framebuffer_bytes =
        page_columns *
        page_rows *
        EF2_GS_TEXTURE_PAGE_BYTES;

    if (framebuffer_bytes == 0u ||
        framebuffer_bytes * 2u >
            EF2_GS_VRAM_BYTES)
        return -6;

    ef2_video_framebuffer_bytes =
        framebuffer_bytes;
    ef2_video_framebuffer_base[0] = 0u;
    ef2_video_framebuffer_base[1] =
        framebuffer_bytes;

    ef2_video_display_buffer = 0u;
    ef2_video_draw_buffer = 0u;
    ef2_video_double_buffered = 0u;
    ef2_video_present_count = 0u;
    ef2_video_vsync_timeouts = 0u;

    ef2_video_active_config.standard = standard;
    ef2_video_active_config.interlaced =
        config->interlaced;
    ef2_video_active_config.field_mode =
        config->field_mode;
    ef2_video_active_config.framebuffer_format =
        format;
    ef2_video_active_config.framebuffer_width =
        ef2_video_width;
    ef2_video_active_config.framebuffer_height =
        ef2_video_height;

    ef2_video_active_layout.format = format;
    ef2_video_active_layout.width =
        ef2_video_width;
    ef2_video_active_layout.height =
        ef2_video_height;
    ef2_video_active_layout.buffer_width =
        (ef2_u8)(ef2_video_width / 64u);
    ef2_video_active_layout.gs_psm =
        framebuffer_psm;
    ef2_video_active_layout.reserved = 0u;
    ef2_video_active_layout.bytes_per_buffer =
        framebuffer_bytes;
    ef2_video_active_layout.buffer_base[0] =
        ef2_video_framebuffer_base[0];
    ef2_video_active_layout.buffer_base[1] =
        ef2_video_framebuffer_base[1];

    ef2_profile_reset(
        &ef2_video_vsync_wait_ticks);

    /*
     * Reserve two page-aligned framebuffers even when compatibility
     * single-buffer mode is active. This lets applications enable
     * double buffering later without invalidating texture addresses.
     */
    ef2_video_texture_start =
        framebuffer_bytes * 2u;
    ef2_video_texture_cursor =
        ef2_video_texture_start;
    ef2_video_active_layout.texture_start =
        ef2_video_texture_start;

    *EF2_GS_REG_PMODE = 0;
    ef2_gs_sync();

    *EF2_GS_REG_CSR = EF2_GS_CSR_RESET;
    ef2_gs_sync();

    *EF2_GS_REG_CSR = 0;
    *EF2_GS_REG_IMR = 0x00007F00u;
    ef2_gs_sync();

    ef2_gif_reset();

    ef2_video_gif_dma_fallbacks = 0;
    ef2_video_gif_dma_available =
        ef2_gif_dma_init() == 0 ? 1u : 0u;

    ef2_kernel_set_gs_crt(
        (ef2_s16)1,
        (ef2_s16)standard,
        (ef2_s16)EF2_VIDEO_FIELD);

    ef2_video_set_background(0, 0, 0);

    ef2_video_program_display_buffer(
        ef2_video_display_buffer);

    *EF2_GS_REG_DISPLAY2 =
        ef2_gs_pack_display(
            dx,
            dy,
            (ef2_u8)(horizontal_scale - 1u),
            (ef2_u8)(vertical_scale - 1u),
            2559u,
            (ef2_u16)(default_height - 1u));

    ef2_gs_sync();

    if (ef2_video_clear(0, 0, 0) != 0)
        return -7;

    *EF2_GS_REG_PMODE =
        ef2_gs_pack_pmode(
            0, 1, 1, 1, 0, 0xFF);

    ef2_gs_sync();
    return 0;
}
