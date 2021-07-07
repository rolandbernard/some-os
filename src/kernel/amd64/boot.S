
#define ELFNOTE(name, type, desc)   \
     .section .note.name ;          \
     .align 4 ;                     \
     .long 2f - 1f ;                \
     .long 4f - 3f ;                \
     .long type ;                   \
 1:.asciz #name ;                   \
 2:.align 4 ;                       \
 3:desc ;                           \
 4:.align 4  ;                      \

#define XEN_ELFNOTE_PHYS32_ENTRY 18

ELFNOTE(Xen, XEN_ELFNOTE_PHYS32_ENTRY, .long _start)

.section .text.boot

.global kernelMain
.global _start

_start:
    call kernelMain
1:
    hlt
    jmp 1b

