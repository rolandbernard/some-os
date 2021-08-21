
.option norvc

.section .text
.global syscall

# Lock simple spinlock with fences
lockSpinLock:
    li t0, 1
1:
    amoswap.w t0, t0, (a0)
    fence
    bnez t0, 1b
    ret

# Unlock simple spinlock with fences
unlockSpinLock:
    fence
    amoswap.w zero, zero, (a0)
    ret

