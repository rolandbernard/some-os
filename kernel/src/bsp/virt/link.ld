
ENTRY(_start)

MEMORY {
    ram (wxa) : ORIGIN = 0x80000000, LENGTH = 128M
}

PHDRS {
    text PT_LOAD;
    rodata PT_LOAD;
    data PT_LOAD;
    bss PT_LOAD;
    rtdebug PT_LOAD;
}

SECTIONS {
    . = ORIGIN(ram);

    .text : {
        PROVIDE(__text_start = .);
        *(.text.boot)
        *(.text .text*)
        PROVIDE(__text_end = .);
    } >ram AT>ram :text

    .eh_frame : {
        . = ALIGN(4096);
        PROVIDE(__eh_frame_start = .);
        *(.eh_frame .eh_frame*)
        . += 8;
        PROVIDE(__eh_frame_end = .);
    } >ram AT>ram :rodata

    .rodata : {
        PROVIDE(__rodata_start = .);
        *(.got .got*)
        *(.rodata .rodata*)
        *(.srodata .srodata*)
        PROVIDE(__rodata_end = .);
    } >ram AT>ram :rodata

    .data : {
        . = ALIGN(4096);
        PROVIDE(__data_start = .);
        *(.sdata .sdata*)
        *(.data .data*)
        PROVIDE(__data_end = .);
    } >ram AT>ram :data

    .bss : {
        . = ALIGN(8);
        PROVIDE(__bss_start = .);
        *(.sbss .sbss*)
        *(.bss .bss*)
        . += 8;
        . = ALIGN(8);
        PROVIDE(__bss_end = .);
    } >ram AT>ram :bss

    .rtdebug : {
        PROVIDE(__rtdebug_start = .);
        *(.rtdebug)
        PROVIDE(__rtdebug_end = .);
    } >ram AT>ram :rtdebug

    PROVIDE(__global_pointer = __rodata_start);

    PROVIDE(__stack_top    = ORIGIN(ram) + LENGTH(ram));
    PROVIDE(__stack_bottom = __stack_top - 0x10000);

    PROVIDE(__heap_start = __rtdebug_end);
    PROVIDE(__heap_end   = __stack_bottom);
}

