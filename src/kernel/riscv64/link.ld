
ENTRY( _start )

MEMORY {
    ram  (wxa) : ORIGIN = 0x80000000, LENGTH = 128M
}

PHDRS {
    text PT_LOAD;
    data PT_LOAD;
    bss PT_LOAD;
}

SECTIONS {
    .text : {
        *(.text.boot)
        *(.text.*)
    } >ram AT>ram :text

    .eh_frame : {
        *(.rodata .rodata.*)
    } >ram AT>ram :text

    PROVIDE(__global_pointer = .);

    .rodata : {
        *(.rodata .rodata.*)
    } >ram AT>ram :text

    .data : {
        . = ALIGN(4K);
        *(.sdata .sdata*)
        *(.data .data.*)
    } >ram AT>ram :data

    .bss ALIGN(8): {
        PROVIDE(__bss_start = .);
        *(.sbss .sbss.*)
        *(.bss .bss.*)
        . += 8;
        . = ALIGN(8);
        PROVIDE(__bss_end = .);
    } >ram AT>ram :bss

    PROVIDE(__stack_top  = ORIGIN(ram) + LENGTH(ram));
}


