#ifndef EF2_COMPAT_SETJMP_H
#define EF2_COMPAT_SETJMP_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * GCC's nonlocal-goto builtins require five pointer-sized words.
 * Keep the requested standard longjmp return value beside that opaque state.
 */
typedef struct {
    __UINTPTR_TYPE__ state[5];
    volatile int value;
} ef2_jmp_state;

typedef ef2_jmp_state jmp_buf[1];

#define setjmp(environment) \
    (__builtin_setjmp((environment)[0].state) \
        ? (environment)[0].value \
        : 0)

void longjmp(
    jmp_buf environment,
    int value) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif
