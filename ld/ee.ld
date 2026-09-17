ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x00100000;

    .text : ALIGN(16)
    {
        KEEP(*(.text.start))
        *(.text)
        *(.text.*)
    } :text

    .rodata : ALIGN(16)
    {
        *(.rodata)
        *(.rodata.*)
    } :text

    .data : ALIGN(16)
    {
        *(.data)
        *(.data.*)
    } :data

    .sdata : ALIGN(16)
    {
        *(.sdata)
        *(.sdata.*)
    } :data

    .bss (NOLOAD) : ALIGN(16)
    {
        __ef2_bss_start = .;
        *(.sbss)
        *(.sbss.*)
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __ef2_bss_end = .;
    } :data

    .ef2_stack (NOLOAD) : ALIGN(16)
    {
        __ef2_stack_bottom = .;
        . += 64K;
        __ef2_stack_top = .;
    } :data

    __ef2_image_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}
