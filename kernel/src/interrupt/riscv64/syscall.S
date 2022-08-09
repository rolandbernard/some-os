
.section .text

# Run ecall and then return
.global syscall
syscall:
.cfi_startproc
    ecall
    ret
.cfi_endproc

