
.section .text

.global lockUnsafeLock
lockUnsafeLock:
.cfi_startproc
    li t0, 1
1:
    amoswap.w t0, t0, (a0)
    fence
    bnez t0, 1b
    ret
.cfi_endproc

.global tryLockingUnsafeLock
tryLockingUnsafeLock:
.cfi_startproc
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
.cfi_endproc

.global unlockUnsafeLock
unlockUnsafeLock:
.cfi_startproc
    fence
    amoswap.w zero, zero, (a0)
    ret
.cfi_endproc

