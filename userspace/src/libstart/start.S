
.section .text
.global _start

_start:
    la gp, __global_pointer
    call main
    j syscall_exit
    ret

