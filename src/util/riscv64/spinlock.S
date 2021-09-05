
.section .text
.global lockSpinLock
.global unlockSpinLock

lockSpinLock:
    li t0, 1
1:
    amoswap.w t0, t0, (a0)
    fence
    beqz t0, 1f
    call yield
    j 1b
1:
    ret

unlockSpinLock:
    fence
    amoswap.w zero, zero, (a0)
    ret

