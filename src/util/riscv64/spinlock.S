
.section .text
.global lockSpinLock
.global unlockSpinLock

lockSpinLock:
    li t0, 1
1:
    amoswap.w t0, t0, (a0)
    fence
    bnez t0, 1b
    ret

unlockSpinLock:
    fence
    amoswap.w zero, zero, (a0)
    ret

