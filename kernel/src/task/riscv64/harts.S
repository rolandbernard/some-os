
.altmacro

.section .text
.global readMhartid
.global readSscratch
.global writeSscratch
.global loadTrapFrame

readMhartid:
    csrr a0, mhartid
    ret

readSscratch:
    csrr a0, sscratch
    ret

writeSscratch:
    csrw sscratch, a0
    ret

.macro save_gp i, basereg
    sd x\i, ((\i)*8)(\basereg)
.endm
.macro load_gp i, basereg
    ld x\i, ((\i)*8)(\basereg)
.endm
.macro save_fp i, basereg
    fsd f\i, ((32+(\i))*8)(\basereg)
.endm
.macro load_fp i, basereg
    fld f\i, ((32+(\i))*8)(\basereg)
.endm

# Save the current state to TrapFrame in a1 and load from a TrapFrame in a0.
# Because this is only to be used inside the kernel, we can assume that we do not have to
# preserve caller saved registers.
loadTrapFrame:
# Saving to a1
    # Save integer registers
.set i, 1
.rept 31
    save_gp %i, a1
    .set i, i+1
.endr
    # Save floating point registers
.set i, 0
.rept 32
    save_fp %i, a1
    .set i, i+1
.endr
    la t1, 1f # Save address we want to return to
    sd t1, 512(a3)
# Loading from a0
    # Restore floating point registers
.set i, 0
.rept 32
    load_fp %i, a0
    .set i, i+1
.endr
    # Restore integer registers
.set i, 2
.rept 8 # Load up to (and not including) a0
    load_gp %i, a0
    .set i, i+1
.endr
    .set i, i+1
.rept 21 # Load after (and not including) a0
    load_gp %i, a0
    .set i, i+1
.endr
    ld ra, 512(a0) # Load return address
1:
    ret

