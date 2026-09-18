#include <ef2/heap.h>
#include <ef2/kernel.h>

extern ef2_u8 __ef2_image_end[];

int ef2_heap_init_default(void)
{
    ef2_u32 start;
    ef2_u32 end;

    start =
        ((ef2_u32)__ef2_image_end + 63u) &
        ~63u;

    /*
     * -1 asks the EE kernel to extend the process heap to the safe end
     * of user memory, matching the conventional PS2 startup behavior.
     */
    ef2_kernel_setup_heap(
        (void *)start,
        -1);

    end = (ef2_u32)ef2_kernel_end_of_heap();

    if (end <= start)
        return -1;

    return ef2_heap_init(
        (void *)start,
        end - start);
}
