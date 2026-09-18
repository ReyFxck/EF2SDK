#include <ef2/gif.h>
#include <ef2/gs.h>
#include <ef2/kernel.h>
#include <ef2/video.h>

#define EF2_GS_TEXTURE_PAGE_BYTES 8192u
#define EF2_GS_TEXTURE_PAGE_WIDTH 64u
#define EF2_GS_TEXTURE_PAGE_HEIGHT 32u
#define EF2_GIF_IMAGE_MAX_QWORDS 0x7FFFu

static ef2_u16 ef2_video_width;
static ef2_u16 ef2_video_height;
static ef2_u32 ef2_video_gif_dma_available;
static ef2_u32 ef2_video_gif_dma_fallbacks;
static ef2_u32 ef2_video_texture_start;
static ef2_u32 ef2_video_texture_cursor;
static ef2_u32 ef2_video_clut_stage[256] EF2_ALIGN(16);

static int ef2_video_standard_valid(
    ef2_video_standard standard)
{
    return standard == EF2_VIDEO_NTSC ||
           standard == EF2_VIDEO_PAL;
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
            0,
            (ef2_u8)(ef2_video_width / 64u),
            EF2_GS_PSMCT32,
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
         texture->psm != EF2_GS_PSMT8) ||
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
            0,
            (ef2_u8)(ef2_video_width / 64u),
            EF2_GS_PSMCT32,
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
    if (texture ==
        (const ef2_video_texture *)0)
        return -1;

    return ef2_video_draw_texture_region(
        texture,
        0,
        0,
        texture->width,
        texture->height,
        x,
        y,
        width,
        height,
        0x80,
        0x80,
        0x80,
        0x80,
        0);
}

int ef2_video_init(
    const ef2_video_config *config)
{
    ef2_u16 dx;
    ef2_u16 dy;
    ef2_u32 framebuffer_bytes;

    if (config ==
        (const ef2_video_config *)0)
        return -1;

    if (!ef2_video_standard_valid(
            config->standard))
        return -2;

    if (!config->interlaced ||
        config->field_mode != EF2_VIDEO_FIELD)
        return -3;

    if (config->standard == EF2_VIDEO_PAL) {
        ef2_video_width = 640;
        ef2_video_height = 512;
        dx = 680;
        dy = 72;
    } else {
        ef2_video_width = 640;
        ef2_video_height = 448;
        dx = 656;
        dy = 36;
    }

    framebuffer_bytes =
        (ef2_u32)ef2_video_width *
        (ef2_u32)ef2_video_height * 4u;

    ef2_video_texture_start =
        ef2_align_up_u32(
            framebuffer_bytes,
            EF2_GS_TEXTURE_PAGE_BYTES);
    ef2_video_texture_cursor =
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
        (ef2_s16)config->standard,
        (ef2_s16)EF2_VIDEO_FIELD);

    ef2_video_set_background(0, 0, 0);

    *EF2_GS_REG_DISPFB2 =
        ef2_gs_pack_dispfb(
            0,
            (ef2_u8)(
                ef2_video_width / 64u),
            EF2_GS_PSMCT32,
            0,
            0);

    *EF2_GS_REG_DISPLAY2 =
        ef2_gs_pack_display(
            dx,
            dy,
            3,
            0,
            (ef2_u16)(
                ef2_video_width * 4u - 1u),
            (ef2_u16)(
                ef2_video_height - 1u));

    ef2_gs_sync();

    if (ef2_video_clear(0, 0, 0) != 0)
        return -4;

    *EF2_GS_REG_PMODE =
        ef2_gs_pack_pmode(
            0, 1, 1, 1, 0, 0xFF);

    ef2_gs_sync();
    return 0;
}
