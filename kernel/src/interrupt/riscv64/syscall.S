
.section .text
.global syscall

# Run ecall and then return
syscall:
    ecall
    ret

