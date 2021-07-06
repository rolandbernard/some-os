
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

.code64

ELFNOTE(Xen, XEN_ELFNOTE_PHYS32_ENTRY, .long _start)

.section .text
.global kernelMain
.global _start

.section .text.boot
_start:
    call kernelMain

