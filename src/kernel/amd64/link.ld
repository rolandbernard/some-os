
ENTRY( _start )

SECTIONS {
    . = 1M;
    .text : {
        *(.text.boot)
        *(.text*)
    }
    . = ALIGN(4K);
    .data : { *(.data*) }
    .bss : { *(.bss*) }
    .notes : { *(.note*) }
}

