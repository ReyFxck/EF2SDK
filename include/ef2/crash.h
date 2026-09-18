#ifndef EF2_CRASH_H
#define EF2_CRASH_H

#include <ef2/base.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    EF2_CRASH_GPR_ZERO = 0,
    EF2_CRASH_GPR_AT = 1,
    EF2_CRASH_GPR_V0 = 2,
    EF2_CRASH_GPR_V1 = 3,
    EF2_CRASH_GPR_A0 = 4,
    EF2_CRASH_GPR_A1 = 5,
    EF2_CRASH_GPR_A2 = 6,
    EF2_CRASH_GPR_A3 = 7,
    EF2_CRASH_GPR_T0 = 8,
    EF2_CRASH_GPR_T1 = 9,
    EF2_CRASH_GPR_T2 = 10,
    EF2_CRASH_GPR_T3 = 11,
    EF2_CRASH_GPR_T4 = 12,
    EF2_CRASH_GPR_T5 = 13,
    EF2_CRASH_GPR_T6 = 14,
    EF2_CRASH_GPR_T7 = 15,
    EF2_CRASH_GPR_S0 = 16,
    EF2_CRASH_GPR_S1 = 17,
    EF2_CRASH_GPR_S2 = 18,
    EF2_CRASH_GPR_S3 = 19,
    EF2_CRASH_GPR_S4 = 20,
    EF2_CRASH_GPR_S5 = 21,
    EF2_CRASH_GPR_S6 = 22,
    EF2_CRASH_GPR_S7 = 23,
    EF2_CRASH_GPR_T8 = 24,
    EF2_CRASH_GPR_T9 = 25,
    EF2_CRASH_GPR_K0 = 26,
    EF2_CRASH_GPR_K1 = 27,
    EF2_CRASH_GPR_GP = 28,
    EF2_CRASH_GPR_SP = 29,
    EF2_CRASH_GPR_FP = 30,
    EF2_CRASH_GPR_RA = 31
};

typedef struct {
    ef2_u64 gpr[32];
    ef2_u32 status;
    ef2_u32 cause;
    ef2_u32 epc;
    ef2_u32 error_epc;
    ef2_u32 badvaddr;
    ef2_u32 reserved[3];
} ef2_crash_frame;

/*
 * Installs EF2SDK's level-1 crash vector for TLB and common synchronous
 * exceptions. It is opt-in so applications that own those vectors can keep
 * their handlers.
 */
int ef2_crash_install(void);

int ef2_crash_is_installed(void);

/* Deliberately raises a trap exception. Intended only for diagnostics. */
EF2_NORETURN void ef2_crash_trigger_test(void);

#ifdef __cplusplus
}
#endif

#endif
