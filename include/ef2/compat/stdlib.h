#ifndef EF2_COMPAT_STDLIB_H
#define EF2_COMPAT_STDLIB_H

#include <ef2/libc.h>

typedef ef2_size_t size_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t count, size_t size);
void *realloc(void *ptr, size_t size);

#endif
