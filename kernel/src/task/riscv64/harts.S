
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

# Save the current state to TrapFrame in a1 and load from a TrapFrame in a0.
# Because this is only to be used inside the kernel, we can assume that we do not have to
# preserve caller saved registers and floating point registers.
.global swapTrapFrame
swapTrapFrame:
.cfi_startproc
# Saving to a1
    save_gp 2, a1 # sp
    save_gp 3, a1 # gp
    save_gp 4, a1 # tp
    save_gp 8, a1 # s0
    save_gp 9, a1 # s1
.set i, 18
.rept 10
    save_gp %i, a1 # s2-11
    .set i, i+1
.endr
    sd ra, 512(a1) # Save address we want to return to
# Loading from a0
    load_gp 1, a0
    load_gp 2, a0
    load_gp 3, a0
    load_gp 4, a0
    load_gp 8, a0
    load_gp 9, a0
.set i, 18
.rept 10
    load_gp %i, a0
    .set i, i+1
.endr
    ld a1, 512(a0) # Load return address
    jr a1
.cfi_endproc

.global saveStateToFrame
saveStateToFrame:
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

