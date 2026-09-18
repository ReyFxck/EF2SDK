#include <setjmp.h>

static jmp_buf g_environment;
static volatile int g_marker;

static void jump_from_nested_call(int value)
{
    g_marker = 0x1234;
    longjmp(g_environment, value);
}

int main(void)
{
    int value;

    g_marker = 0;
    value = setjmp(g_environment);

    if (value == 0) {
        jump_from_nested_call(37);
        return 1;
    }

    if (value != 37)
        return 2;

    if (g_marker != 0x1234)
        return 3;

    value = setjmp(g_environment);

    if (value == 0) {
        longjmp(g_environment, 0);
        return 4;
    }

    if (value != 1)
        return 5;

    return 0;
}
