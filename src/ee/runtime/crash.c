#include <ef2/crash.h>
#include <ef2/debug.h>
#include <ef2/kernel.h>
#include <ef2/video.h>

#define EF2_CRASH_LOG_BYTES 1024u
#define EF2_CRASH_SCREEN_W 320u
#define EF2_CRASH_SCREEN_H 224u

extern void ef2_crash_vector(void);

static ef2_u32 g_crash_installed;
static volatile ef2_u32 g_crash_active;

static ef2_u32 g_crash_pixels[
    EF2_CRASH_SCREEN_W * EF2_CRASH_SCREEN_H]
    EF2_ALIGN(16);

static char g_crash_recent[EF2_CRASH_LOG_BYTES];

static const ef2_u8 g_font_digits[10][5] = {
    {7,5,5,5,7}, {2,6,2,2,7},
    {7,1,7,4,7}, {7,1,7,1,7},
    {5,5,7,1,1}, {7,4,7,1,7},
    {7,4,7,5,7}, {7,1,1,1,1},
    {7,5,7,5,7}, {7,5,7,1,7}
};

static const ef2_u8 g_font_letters[26][5] = {
    {2,5,7,5,5}, {6,5,6,5,6},
    {7,4,4,4,7}, {6,5,5,5,6},
    {7,4,6,4,7}, {7,4,6,4,4},
    {7,4,5,5,7}, {5,5,7,5,5},
    {7,2,2,2,7}, {1,1,1,5,7},
    {5,5,6,5,5}, {4,4,4,4,7},
    {5,7,7,5,5}, {5,7,7,7,5},
    {7,5,5,5,7}, {7,5,7,4,4},
    {7,5,5,7,1}, {6,5,6,5,5},
    {7,4,7,1,7}, {7,2,2,2,2},
    {5,5,5,5,7}, {5,5,5,5,2},
    {5,5,7,7,5}, {5,5,2,5,5},
    {5,5,2,2,2}, {7,1,2,4,7}
};

static ef2_u8 glyph_row(char character, ef2_u32 row)
{
    if (row >= 5u)
        return 0;

    if (character >= 'a' && character <= 'z')
        character = (char)(character - 'a' + 'A');

    if (character >= '0' && character <= '9')
        return g_font_digits[
            (ef2_u32)(character - '0')][row];

    if (character >= 'A' && character <= 'Z')
        return g_font_letters[
            (ef2_u32)(character - 'A')][row];

    switch (character) {
        case '[': {
            static const ef2_u8 p[5] = {6,4,4,4,6};
            return p[row];
        }
        case ']': {
            static const ef2_u8 p[5] = {3,1,1,1,3};
            return p[row];
        }
        case '=': {
            static const ef2_u8 p[5] = {0,7,0,7,0};
            return p[row];
        }
        case '-': {
            static const ef2_u8 p[5] = {0,0,7,0,0};
            return p[row];
        }
        case ':': {
            static const ef2_u8 p[5] = {0,2,0,2,0};
            return p[row];
        }
        case '.': {
            static const ef2_u8 p[5] = {0,0,0,0,2};
            return p[row];
        }
        case '/': {
            static const ef2_u8 p[5] = {1,1,2,4,4};
            return p[row];
        }
        case '_': {
            static const ef2_u8 p[5] = {0,0,0,0,7};
            return p[row];
        }
        case '<': {
            static const ef2_u8 p[5] = {1,2,4,2,1};
            return p[row];
        }
        case '>': {
            static const ef2_u8 p[5] = {4,2,1,2,4};
            return p[row];
        }
        case '#': {
            static const ef2_u8 p[5] = {5,7,5,7,5};
            return p[row];
        }
        case ' ':
            return 0;
        default: {
            static const ef2_u8 p[5] = {7,1,2,0,2};
            return p[row];
        }
    }
}

static void pixel_put(
    ef2_u32 x,
    ef2_u32 y,
    ef2_u32 color)
{
    if (x >= EF2_CRASH_SCREEN_W ||
        y >= EF2_CRASH_SCREEN_H)
        return;

    g_crash_pixels[
        y * EF2_CRASH_SCREEN_W + x] = color;
}

static void screen_fill(ef2_u32 color)
{
    ef2_u32 i;

    for (i = 0;
         i < EF2_CRASH_SCREEN_W * EF2_CRASH_SCREEN_H;
         ++i)
        g_crash_pixels[i] = color;
}

static void draw_character(
    ef2_u32 x,
    ef2_u32 y,
    char character,
    ef2_u32 color)
{
    ef2_u32 row;

    for (row = 0; row < 5u; ++row) {
        ef2_u8 bits =
            glyph_row(character, row);
        ef2_u32 column;

        for (column = 0; column < 3u; ++column) {
            if ((bits & (1u << (2u - column))) != 0u)
                pixel_put(
                    x + column,
                    y + row,
                    color);
        }
    }
}

static void draw_text(
    ef2_u32 x,
    ef2_u32 y,
    const char *text,
    ef2_u32 color,
    ef2_u32 max_columns)
{
    ef2_u32 start_x = x;
    ef2_u32 column = 0;

    if (text == (const char *)0)
        return;

    while (*text != '\0' &&
           y + 5u < EF2_CRASH_SCREEN_H) {
        char character = *text++;

        if (character == '\r')
            continue;

        if (character == '\n' ||
            column >= max_columns) {
            x = start_x;
            y += 6u;
            column = 0;

            if (character == '\n')
                continue;
        }

        draw_character(
            x,
            y,
            character,
            color);

        x += 4u;
        ++column;
    }
}

static char *append_char(
    char *cursor,
    char *end,
    char value)
{
    if (cursor < end)
        *cursor++ = value;

    return cursor;
}

static char *append_text(
    char *cursor,
    char *end,
    const char *text)
{
    while (text != (const char *)0 &&
           *text != '\0')
        cursor = append_char(
            cursor,
            end,
            *text++);

    return cursor;
}

static char *append_hex32(
    char *cursor,
    char *end,
    ef2_u32 value)
{
    static const char digits[] =
        "0123456789ABCDEF";
    ef2_s32 shift;

    cursor = append_text(
        cursor,
        end,
        "0x");

    for (shift = 28; shift >= 0; shift -= 4) {
        cursor = append_char(
            cursor,
            end,
            digits[(value >> shift) & 0x0Fu]);
    }

    return cursor;
}

static void crash_log_frame(
    const ef2_crash_frame *frame)
{
    char line[256];
    char *cursor = line;
    char *end = line + sizeof(line) - 1;
    ef2_u32 exception_code =
        (frame->cause >> 2) & 0x1Fu;

    cursor = append_text(
        cursor, end, "[EF2][CRASH] EXC=");
    cursor = append_hex32(
        cursor, end, exception_code);
    cursor = append_text(
        cursor, end, " CAUSE=");
    cursor = append_hex32(
        cursor, end, frame->cause);
    cursor = append_text(
        cursor, end, " EPC=");
    cursor = append_hex32(
        cursor, end, frame->epc);
    cursor = append_text(
        cursor, end, " BADV=");
    cursor = append_hex32(
        cursor, end, frame->badvaddr);
    cursor = append_text(
        cursor, end, " SP=");
    cursor = append_hex32(
        cursor,
        end,
        (ef2_u32)frame->gpr[EF2_CRASH_GPR_SP]);
    cursor = append_text(
        cursor, end, " RA=");
    cursor = append_hex32(
        cursor,
        end,
        (ef2_u32)frame->gpr[EF2_CRASH_GPR_RA]);
    cursor = append_text(
        cursor, end, " GP=");
    cursor = append_hex32(
        cursor,
        end,
        (ef2_u32)frame->gpr[EF2_CRASH_GPR_GP]);
    cursor = append_char(
        cursor, end, '\n');
    *cursor = '\0';

    (void)ef2_debug_write_raw(
        line,
        (ef2_size_t)(cursor - line));
}

static void draw_register_line(
    ef2_u32 y,
    const char *label,
    ef2_u32 value)
{
    char line[32];
    char *cursor = line;
    char *end = line + sizeof(line) - 1;

    cursor = append_text(
        cursor, end, label);
    cursor = append_char(
        cursor, end, ' ');
    cursor = append_hex32(
        cursor, end, value);
    *cursor = '\0';

    draw_text(
        8,
        y,
        line,
        0x80FFFFFFu,
        76u);
}

static const char *recent_log_start(
    const char *buffer,
    ef2_size_t length,
    ef2_u32 lines)
{
    const char *cursor =
        buffer + length;

    while (cursor > buffer && lines != 0u) {
        --cursor;

        if (*cursor == '\n')
            --lines;
    }

    if (cursor != buffer &&
        *cursor == '\n')
        ++cursor;

    return cursor;
}

static void crash_draw_screen(
    const ef2_crash_frame *frame)
{
    ef2_video_texture texture;
    ef2_u16 width;
    ef2_u16 height;
    ef2_size_t recent_length;
    const char *recent;
    ef2_u32 exception_code =
        (frame->cause >> 2) & 0x1Fu;

    if (ef2_video_get_size(
            &width,
            &height) != 0)
        return;

    (void)ef2_video_set_double_buffering(0);
    screen_fill(0x80000038u);

    draw_text(
        8, 6,
        "EF2 CRASH",
        0x8000D8FFu,
        76u);

    draw_register_line(
        20, "EXC", exception_code);
    draw_register_line(
        28, "CAUSE", frame->cause);
    draw_register_line(
        36, "EPC", frame->epc);
    draw_register_line(
        44, "BADV", frame->badvaddr);
    draw_register_line(
        52, "SP",
        (ef2_u32)frame->gpr[EF2_CRASH_GPR_SP]);
    draw_register_line(
        60, "RA",
        (ef2_u32)frame->gpr[EF2_CRASH_GPR_RA]);
    draw_register_line(
        68, "GP",
        (ef2_u32)frame->gpr[EF2_CRASH_GPR_GP]);

    draw_text(
        8, 84,
        "RECENT RAM LOG",
        0x8000D8FFu,
        76u);

    recent_length =
        ef2_debug_copy_recent(
            g_crash_recent,
            sizeof(g_crash_recent));

    recent = recent_log_start(
        g_crash_recent,
        recent_length,
        20u);

    draw_text(
        8, 94,
        recent,
        0x80E8E8E8u,
        76u);

    ef2_video_reset_texture_allocator();

    if (ef2_video_upload_rgba32(
            &texture,
            g_crash_pixels,
            EF2_CRASH_SCREEN_W,
            EF2_CRASH_SCREEN_H) == 0) {
        (void)ef2_video_draw_texture(
            &texture,
            0,
            0,
            width,
            height);
    } else {
        (void)ef2_video_clear(
            120, 0, 0);
    }
}

int ef2_crash_install(void)
{
    ef2_s32 cause;

    if (g_crash_installed)
        return 0;

    for (cause = 1; cause <= 3; ++cause) {
        ef2_kernel_set_vtlb_refill_handler(
            cause,
            ef2_crash_vector);
    }

    for (cause = 4; cause <= 7; ++cause) {
        ef2_kernel_set_v_common_handler(
            cause,
            ef2_crash_vector);
    }

    for (cause = 10; cause <= 13; ++cause) {
        ef2_kernel_set_v_common_handler(
            cause,
            ef2_crash_vector);
    }

    ef2_kernel_flush_cache(0);
    ef2_kernel_flush_cache(2);

    g_crash_installed = 1u;
    return 0;
}

int ef2_crash_is_installed(void)
{
    return g_crash_installed != 0u;
}

EF2_NORETURN void ef2_crash_dispatch(
    const ef2_crash_frame *frame)
{
    if (g_crash_active) {
        for (;;)
            __asm__ volatile("nop");
    }

    g_crash_active = 1u;

    if (frame != (const ef2_crash_frame *)0) {
        crash_log_frame(frame);
        crash_draw_screen(frame);
    }

    for (;;)
        __asm__ volatile("nop");
}
