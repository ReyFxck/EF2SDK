#ifndef EF2_RUNTIME_H
#define EF2_RUNTIME_H

#include <ef2/base.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EF2_RUNTIME_ARGS_NONE = 0,
    EF2_RUNTIME_ARGS_DIRECT = 1,
    EF2_RUNTIME_ARGS_SARGS_START = 2,
    EF2_RUNTIME_ARGS_SARGS = 3
} ef2_runtime_args_source;

typedef struct {
    ef2_s32 argc;
    char **argv;
    ef2_u32 raw_a0;
    ef2_u32 raw_a1;
    ef2_runtime_args_source source;
} ef2_runtime_args;

ef2_s32 ef2_runtime_argc(void);
char *const *ef2_runtime_argv(void);

void ef2_runtime_get_args(ef2_runtime_args *args);

EF2_NORETURN void ef2_runtime_exit(ef2_s32 status);

#ifdef __cplusplus
}
#endif

#endif
