#include "zlib.h"

static const unsigned char source[] =
    "EF2SDK zlib smoke: "
    "PlayStation 2 compression without PS2SDK libc. "
    "EF2SDK zlib smoke: "
    "PlayStation 2 compression without PS2SDK libc.";

int main(void)
{
    unsigned char compressed[512];
    unsigned char restored[512];
    uLong compressed_size =
        (uLong)sizeof(compressed);
    uLong restored_size =
        (uLong)sizeof(restored);
    uLong source_size =
        (uLong)(sizeof(source) - 1u);
    uLong i;
    int result;

    result = compress2(
        compressed,
        &compressed_size,
        source,
        source_size,
        Z_BEST_COMPRESSION);

    if (result != Z_OK)
        return 1;

    if (compressed_size >= source_size)
        return 2;

    result = uncompress(
        restored,
        &restored_size,
        compressed,
        compressed_size);

    if (result != Z_OK)
        return 3;

    if (restored_size != source_size)
        return 4;

    for (i = 0; i < source_size; ++i) {
        if (restored[i] != source[i])
            return 5;
    }

    return 0;
}
