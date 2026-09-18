#include <ef2/heap.h>
#include <ef2/kernel.h>
#include <ef2/runtime.h>

#define EF2_RUNTIME_MAX_ARGS 16
#define EF2_RUNTIME_USER_MIN 0x00100000u
#define EF2_RUNTIME_USER_MAX 0x02000000u

typedef struct {
    ef2_s32 argc;
    char *argv[EF2_RUNTIME_MAX_ARGS];
    char payload[256];
} ef2_loader_sargs;

typedef struct {
    ef2_s32 pid;
    ef2_loader_sargs args;
} ef2_loader_sargs_start;

static ef2_s32 g_argc;
static char **g_argv;
static ef2_u32 g_raw_a0;
static ef2_u32 g_raw_a1;
static ef2_runtime_args_source g_source;

extern int main(int argc, char **argv);

static int ef2_runtime_user_pointer(ef2_u32 value)
{
    return value >= EF2_RUNTIME_USER_MIN &&
           value < EF2_RUNTIME_USER_MAX &&
           (value & 3u) == 0u;
}

static int ef2_runtime_argc_valid(ef2_s32 argc)
{
    return argc >= 0 &&
           argc <= EF2_RUNTIME_MAX_ARGS;
}

void ef2_runtime_capture_loader_args(
    ef2_u32 raw_a0,
    ef2_u32 raw_a1)
{
    g_argc = 0;
    g_argv = (char **)0;
    g_raw_a0 = raw_a0;
    g_raw_a1 = raw_a1;
    g_source = EF2_RUNTIME_ARGS_NONE;

    /*
     * Some lightweight ELF launchers use the ordinary C ABI directly.
     * Keep this first because argc is unambiguously a small integer.
     */
    if (raw_a0 <= EF2_RUNTIME_MAX_ARGS) {
        if (raw_a0 == 0u ||
            ef2_runtime_user_pointer(raw_a1)) {
            g_argc = (ef2_s32)raw_a0;
            g_argv = raw_a0 == 0u
                ? (char **)0
                : (char **)raw_a1;
            g_source = EF2_RUNTIME_ARGS_DIRECT;
            return;
        }
    }

    if (ef2_runtime_user_pointer(raw_a0)) {
        ef2_loader_sargs_start *start =
            (ef2_loader_sargs_start *)raw_a0;
        ef2_s32 argc = start->args.argc;

        /*
         * ps2link/PS2SDK-style launchers pass a pointer whose first word is
         * a PID and whose nested sargs starts at +4.
         */
        if (ef2_runtime_argc_valid(argc)) {
            g_argc = argc;
            g_argv = start->args.argv;
            g_source = EF2_RUNTIME_ARGS_SARGS_START;
            return;
        }

        /*
         * A few custom loaders pass sargs itself. Supporting this costs
         * nothing and keeps the public ABI independent from one launcher.
         */
        {
            ef2_loader_sargs *args =
                (ef2_loader_sargs *)raw_a0;

            if (ef2_runtime_argc_valid(args->argc)) {
                g_argc = args->argc;
                g_argv = args->argv;
                g_source = EF2_RUNTIME_ARGS_SARGS;
            }
        }
    }
}

ef2_s32 ef2_runtime_argc(void)
{
    return g_argc;
}

char *const *ef2_runtime_argv(void)
{
    return g_argv;
}

void ef2_runtime_get_args(ef2_runtime_args *args)
{
    if (args == (ef2_runtime_args *)0)
        return;

    args->argc = g_argc;
    args->argv = g_argv;
    args->raw_a0 = g_raw_a0;
    args->raw_a1 = g_raw_a1;
    args->source = g_source;
}

EF2_NORETURN void ef2_runtime_exit(ef2_s32 status)
{
    ef2_kernel_exit(status);

    /*
     * KExit is expected not to return. Keep a hard stop here for odd
     * kernels/emulators rather than falling through into arbitrary memory.
     */
    for (;;) {
        __asm__ volatile("nop");
    }
}

EF2_NORETURN void ef2_runtime_abort(void)
{
    ef2_runtime_exit(-1);
}

#ifndef EF2_LIBC_NO_STANDARD_ALIASES
EF2_NORETURN void abort(void)
{
    ef2_runtime_abort();
}
#endif

EF2_NORETURN void ef2_runtime_run_main(void)
{
    ef2_s32 status;

    (void)ef2_heap_init_default();

    status = main(g_argc, g_argv);
    ef2_runtime_exit(status);
}
