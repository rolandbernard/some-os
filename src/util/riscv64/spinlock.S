
.section .text
.global lockSpinLock
.global unlockSpinLock

lockSpinLock:
    addi sp, sp, -16
    sd s0, (sp)
    sd ra, 8(sp)
    mv s0, a0
    li t0, 1
1:
    amoswap.w t0, t0, (s0)
    fence
    beqz t0, 1f
    call yield
    j 1b
1:
    ld s0, (sp)
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

unlockSpinLock:
    fence
    amoswap.w zero, zero, (a0)
    ret

