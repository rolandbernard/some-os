
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

tryLockingSpinLock:
    li t0, 1
    amoswap.w t0, t0, (a0)
    fence
    beqz t0, 1f
    li a0, 0
    j 2f
1:
    li a0, 1
2:
    ret

unlockSpinLock:
    fence
    amoswap.w zero, zero, (a0)
    ret

