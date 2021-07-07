
ENTRY( _start )

PHDRS {
    text PT_LOAD;
    data PT_LOAD;
    bss PT_LOAD;
    note PT_NOTE;
}

SECTIONS {
    .note.Xen : {
        *(.note.Xen)
    } :note

    . = 1M;
    .text : {
        *(.text.boot)
        *(.text*)
    } :text
    . = ALIGN(4K);
    .data : {
        *(.data*)
    } :data
    .bss : {
        *(.bss*)
    } :bss
}


