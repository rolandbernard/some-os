
.altmacro

.section .text
.global initTraps
.global enableInterrupts
.global enterUserMode
.global enterKernelMode
.global waitForInterrupt
.global kernelTrapVector

# Simple function to wait for interrupt
waitForInterrupt:
    wfi
    ret

# Initialize CSRs for traps
initTraps:
    la t0, kernelTrapVector
    csrw stvec, t0
    csrw sscratch, zero
    # Delagate all exceptions and interrupts
    li t0, 0xffff
    csrw medeleg, t0
    csrw mideleg, t0
    li t0, 0xffff
    csrw mie, t0
    csrw sie, t0
    ret

// Write to the supervisor interrupt enable register
enableInterrupts:
    csrrsi zero, sstatus, 1 << 1 # Directly enable interrupts
    ret

# Enter as user using trap frame in a0
enterUserMode:
    # 0b00 << 11 MPP (Previous Protection Mode) is 0 (U mode)
    li t0, 0b11 << 11
    csrrc zero, sstatus, t0
    j kernelTrapReturn

# Enter as superior using trap frame in a0
enterKernelMode:
    # 0b01 << 11 MPP (Previous Protection Mode) is 1 (S mode)
    li t0, 0b11 << 11
    csrrc zero, sstatus, t0
    li t0, 0b01 << 11
    csrrs zero, sstatus, t0
    j kernelTrapReturn

.macro save_gp i, basereg=x31
    sd x\i, ((\i)*8)(\basereg)
.endm
.macro load_gp i, basereg=x31
    ld x\i, ((\i)*8)(\basereg)
.endm
.macro save_fp i, basereg=x31
    fsd f\i, ((32+(\i))*8)(\basereg)
.endm
.macro load_fp i, basereg=x31
    fld f\i, ((32+(\i))*8)(\basereg)
.endm

# Return from trap using trap frame in a0
kernelTrapReturn:
    ld t0, 512(a0)
    csrw sepc, t0
    ld t0, 520(a0)
    csrw satp, t0
    csrw sscratch, a0
    li t0, 0b11 << 13
    csrrc zero, sstatus, t0
    li t0, 0b01 << 13 | 1 << 5 # Enable FPU and SPIE
    csrrs zero, sstatus, t0
    mv x31, a0 # Load into x31 because it is loaded last
    # Restore floating point registers
.set i, 0
.rept 32
    load_fp %i
    .set i, i+1
.endr
    # Restore integer registers
.set i, 1
.rept 31
    load_gp %i
    .set i, i+1
.endr
    sret

.align 4
# Called on supervisor tarps, call out to C code
kernelTrapVector:
    csrrw x31, sscratch, x31 # Swap scratch (register save location) with x31
    beqz x31, 1f
    # Restore integer registers
.set i, 1
.rept 30
    save_gp %i
    .set i, i+1
.endr
    # Save floating point registers
.set i, 0
.rept 32
    save_fp %i
    .set i, i+1
.endr
    mv a3, x31
    ld t0, 0(x31)
    csrrw x31, sscratch, t0
    save_gp 31 a3
    csrr t1, sepc
    sd t1, 512(a3)
    csrr t1, satp
    sd t1, 520(a3)
    beqz t0, 2f
    # Load global an stack pointer
    load_gp 2, t0
    load_gp 3, t0
    j 3f
1:
    mv a3, zero
2:
    la gp, __global_pointer
    la sp, __stack_top
3:
    csrr a0, scause
    csrr a1, sepc
    csrr a2, stval
    jal kernelTrap
    mv a0, a3
    j kernelTrapReturn

