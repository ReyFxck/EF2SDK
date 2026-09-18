#include <ef2/compat/setjmp.h>

void longjmp(
    jmp_buf environment,
    int value)
{
    environment[0].value =
        value == 0 ? 1 : value;

    /*
     * GCC requires the second argument to __builtin_longjmp to be exactly 1.
     * The user-visible value is carried separately in ef2_jmp_state.
     */
    __builtin_longjmp(
        environment[0].state,
        1);
}
