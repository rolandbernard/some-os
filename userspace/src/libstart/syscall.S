
.section .text
.global make_syscall

# Run ecall and then return
make_syscall:
    ecall
    ret

