#include <ef2/base.h>

volatile ef2_u32 ef2_boot_counter;

int main(void)
{
    ef2_boot_counter = 1;

    for (;;) {
        ++ef2_boot_counter;
        __asm__ volatile ("nop");
    }
}
