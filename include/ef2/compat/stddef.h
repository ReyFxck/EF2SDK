#ifndef EF2_COMPAT_STDDEF_H
#define EF2_COMPAT_STDDEF_H

typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void *)0)
#endif
#endif

#define offsetof(type, member) __builtin_offsetof(type, member)

#endif
