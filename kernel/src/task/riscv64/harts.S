
.altmacro

.section .text

.global readMhartid
readMhartid:
.cfi_startproc
    csrr a0, mhartid
    ret
.cfi_endproc

.global readSscratch
readSscratch:
.cfi_startproc
    csrr a0, sscratch
    ret
.cfi_endproc

.global writeSscratch
writeSscratch:
.cfi_startproc
    csrw sscratch, a0
    ret
.cfi_endproc

.macro save_gp i, basereg
    sd x\i, ((\i)*8)(\basereg)
.endm
.macro load_gp i, basereg
    ld x\i, ((\i)*8)(\basereg)
.endm
.macro save_fp i, basereg=x31
    fsd f\i, ((32+(\i))*8)(\basereg)
.endm
.macro load_fp i, basereg=x31
    fld f\i, ((32+(\i))*8)(\basereg)
.endm

.global saveToFrame
saveToFrame:
.cfi_startproc
    mv t0, a0
    save_gp 2, t0 # sp
    save_gp 3, t0 # gp
    save_gp 4, t0 # tp
    save_gp 8, t0 # s0
    save_gp 9, t0 # s1
    li a0, 0
    save_gp 10, t0 # a0
.set i, 18
.rept 10
    save_gp %i, t0 # s2-11
    .set i, i+1
.endr
    sd ra, 512(t0) # Save address we want to return to
    li a0, 1
    ret
.cfi_endproc

.global loadFromFrame
loadFromFrame:
.cfi_startproc
    mv t0, a0
    load_gp 1, t0
    load_gp 2, t0
    load_gp 3, t0
    load_gp 4, t0
    load_gp 8, t0
    load_gp 9, t0
    load_gp 10, t0
.set i, 18
.rept 10
    load_gp %i, t0
    .set i, i+1
.endr
    ld t0, 512(t0)
    jr t0
.cfi_endproc

.global loadAllFromFrame
loadAllFromFrame:
.cfi_startproc
    mv x31, a0
.set i, 0
.rept 32
    load_fp %i, x31
    .set i, i+1
.endr
.set i, 1
.rept 30
    load_gp %i, x31
    .set i, i+1
.endr
    ld x31, 512(x31)
    jr x31
.cfi_endproc

.global callInHart
callInHart:
.cfi_startproc
    mv t0, a0
    csrr a0, sscratch
    load_gp 2, a0
    jr t0
.cfi_endproc

