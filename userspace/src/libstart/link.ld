
ENTRY(_start)

PHDRS {
    text PT_LOAD;
    data PT_LOAD;
    bss PT_LOAD;
}

SECTIONS {
    .text ALIGN(4K): {
        PROVIDE(__text_start = .);
        *(.text .text*)
        PROVIDE(__text_end = .);
    } :text

    .data ALIGN(4K): {
        PROVIDE(__data_start = .);
        *(.got)
        *(.rodata .rodata*)
        *(.sdata .sdata*)
        *(.data .data*)
        PROVIDE(__data_end = .);
    } :data

    .bss ALIGN(4K): {
        PROVIDE(__bss_start = .);
        *(.sbss .sbss*)
        *(.bss .bss*)
        PROVIDE(__bss_end = .);
    } :bss

    PROVIDE(__global_pointer$ = __data_start);
}

