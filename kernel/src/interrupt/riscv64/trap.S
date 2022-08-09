
.altmacro

.section .text

# Simple function to wait for interrupt
.global waitForInterrupt
waitForInterrupt:
.cfi_startproc
    wfi
    ret
.cfi_endproc

# Initialize CSRs for traps
.global initTraps
initTraps:
.cfi_startproc
    la t0, kernelTrapVector
    csrw stvec, t0
    # Delagate all exceptions and interrupts
    li t0, 0xffff
    # NOTE: Delegation disabled for now.
    # (How to handle M-mode interrupt while handling S-mode interrupt?)
    # (SpinLocks are a problem.)
    # csrw medeleg, t0
    # csrw mideleg, t0
    csrw mie, t0
    csrw sie, t0
    # Some things can not be delegated
    la t0, indirectKernelTrapVector
    csrw mtvec, t0
    ret
.cfi_endproc

# Write to the supervisor interrupt enable register
.global enableInterrupts
enableInterrupts:
.cfi_startproc
    csrrsi zero, sstatus, 1 << 1 # Directly enable interrupts (SIE)
    ret
.cfi_endproc

# Enter as user using trap frame in a0
.global enterUserMode
enterUserMode:
.cfi_startproc
    # 0b0 << 8 SPP (Previous Protection Mode) is 0 (U mode)
    li t0, 1 << 8
    csrrc zero, sstatus, t0
    j kernelTrapReturn
.cfi_endproc

# Enter as superior using trap frame in a0
.global enterKernelMode
enterKernelMode:
.cfi_startproc
    # 0b1 << 8 SPP (Previous Protection Mode) is 1 (S mode)
    li t0, 1 << 8
    csrrs zero, sstatus, t0
    j kernelTrapReturn
.cfi_endproc

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
.macro cfi_gp i
.cfi_offset x\i, ((\i)*8)
.endm

# Return from trap using trap frame in a0
kernelTrapReturn:
.cfi_startproc
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
    # There is currently no good ASID support
    sfence.vma zero, zero
    sret
.cfi_endproc

.align 4
indirectKernelTrapVector:
.cfi_startproc
    csrw mscratch, t0
    csrr t0, mcause
    csrw scause, t0
    csrr t0, mepc
    csrw sepc, t0
    csrr t0, mtval
    csrw stval, t0
    csrr t0, mscratch
    j kernelTrapVector
.cfi_endproc

.align 4
# Called on supervisor tarps, save all registers and call out to C code.
.global kernelTrapVector
kernelTrapVector:
.cfi_startproc
# Small section with call frame information for unwinding into the trapped context.
# This is kind of hacky but works with the unwind in libgcc.
.set i, 1
.rept 31
    cfi_gp %i
    .set i, i+1
.endr
.cfi_offset x0, 512
.cfi_return_column x0
    csrrw x31, sscratch, x31 # Swap scratch (with trap frame) with x31
.cfi_def_cfa x31, 0
    beqz x31, 1f
    # Save integer registers
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
.cfi_def_cfa a3, 0
    ld t0, 0(x31)
    bnez t0, 3f
    mv t0, a3
3:
    csrrw x31, sscratch, t0
    save_gp 31 a3
    csrr t1, sepc
    sd t1, 512(a3)
    csrr t1, satp
    sd t1, 520(a3)
    # Load global an stack pointer
    load_gp 2, t0
    load_gp 3, t0
    j 2f
1:
    mv a3, zero
    la gp, __global_pointer
    jal getHartStack
    mv sp, a0
2:
    csrr a0, scause
    csrr a1, sepc
    csrr a2, stval
    mv s0, a3
.cfi_def_cfa s0, 0
    jal kernelTrap
    mv a0, s0
    j kernelTrapReturn
.cfi_endproc

