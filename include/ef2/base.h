#ifndef EF2_BASE_H
#define EF2_BASE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char      ef2_u8;
typedef unsigned short     ef2_u16;
typedef unsigned int       ef2_u32;
typedef unsigned long long ef2_u64;

typedef signed char        ef2_s8;
typedef signed short       ef2_s16;
typedef signed int         ef2_s32;
typedef signed long long   ef2_s64;

#define EF2_NORETURN __attribute__((noreturn))
#define EF2_ALIGN(n) __attribute__((aligned(n)))

#ifdef __cplusplus
}
#endif

#endif
