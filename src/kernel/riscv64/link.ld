
ENTRY(_start)

MEMORY {
    ram (wxa) : ORIGIN = 0x80000000, LENGTH = 128M
}

PHDRS {
    text PT_LOAD;
    data PT_LOAD;
    bss PT_LOAD;
}

SECTIONS {
    .text ALIGN(4k): {
        PROVIDE(__text_start = .);
        *(.text.boot)
        *(.text .text*)
        PROVIDE(__text_end = .);
    } >ram AT>ram :text

    .data ALIGN(4k): {
        PROVIDE(__data_start = .);
        *(.rodata .rodata*)
        *(.sdata .sdata*)
        *(.data .data*)
        PROVIDE(__data_end = .);
    } >ram AT>ram :data

    .bss ALIGN(4k): {
        PROVIDE(__bss_start = .);
        *(.sbss .sbss*)
        *(.bss .bss*)
        . += 8;
        . = ALIGN(8);
        PROVIDE(__bss_end = .);
    } >ram AT>ram :bss

    PROVIDE(__global_pointer = __data_start);

    PROVIDE(__stack_top    = ORIGIN(ram) + LENGTH(ram));
    PROVIDE(__stack_bottom = __stack_top - 0x10000);

    PROVIDE(__heap_start = __bss_end);
    PROVIDE(__heap_end   = __stack_bottom);
}

