
.section .text

# Run ecall and then return
.global syscall
syscall:
    ecall
    ret

