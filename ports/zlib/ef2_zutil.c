/*
 * EF2SDK replacement for zlib's target-dependent zutil.c.
 *
 * This file intentionally provides only the pieces needed by the in-memory
 * deflate/inflate core. The gzip/stdio file layer is not part of this port yet.
 */
#include "zutil.h"

z_const char * const z_errmsg[10] = {
    (z_const char *)"need dictionary",
    (z_const char *)"stream end",
    (z_const char *)"",
    (z_const char *)"file error",
    (z_const char *)"stream error",
    (z_const char *)"data error",
    (z_const char *)"insufficient memory",
    (z_const char *)"buffer error",
    (z_const char *)"incompatible version",
    (z_const char *)""
};

const char * ZEXPORT zlibVersion(void)
{
    return ZLIB_VERSION;
}

uLong ZEXPORT zlibCompileFlags(void)
{
    uLong flags = 0;

    if (sizeof(uInt) == 4)
        flags |= 1u;
    if (sizeof(uLong) == 4)
        flags |= 1u << 2;
    if (sizeof(voidpf) == 4)
        flags |= 1u << 4;
    if (sizeof(z_off_t) == 4)
        flags |= 1u << 6;

    return flags;
}

const char * ZEXPORT zError(int err)
{
    return ERR_MSG(err);
}

voidpf ZLIB_INTERNAL zcalloc(
    voidpf opaque,
    unsigned items,
    unsigned size)
{
    (void)opaque;
    return calloc(items, size);
}

void ZLIB_INTERNAL zcfree(
    voidpf opaque,
    voidpf ptr)
{
    (void)opaque;
    free(ptr);
}
