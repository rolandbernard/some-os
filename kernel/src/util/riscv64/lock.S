
.section .text
.global lockUnsafeLock
.global tryLockingUnsafeLock
.global unlockUnsafeLock

lockLock:
    li t0, 1
1:
    amoswap.w t0, t0, (a0)
    fence
    bnez t0, 1b
    ret

tryLockingLock:
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

unlockLock:
    fence
    amoswap.w zero, zero, (a0)
    ret

