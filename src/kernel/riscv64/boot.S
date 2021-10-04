
.section .text.boot
.global _start

_start:
    # Set global and stack pointer
    la gp, __global_pointer
    la sp, __stack_top

    # Initialize trap data
    la t0, machineTrapVector
    csrw mtvec, t0
    csrw mie, zero
    csrw sscratch, zero

    # Go into infinite loop with all but one hart
    csrr t0, mhartid
    bnez t0, 1f

    # Init traps for the first hart
    jal initTraps

    # Disable address translation
    csrw satp, zero
    # Give S and U mode access to memory
    csrw pmpcfg0, 0xF
    li t0, -1
    csrw pmpaddr0, t0

    # Enter the kernel in S mode and enable FPU
    li t0, 0b11 << 11 | 0b11 << 13
    csrrc zero, mstatus, t0
    li t0, 0b01 << 11 | 0b01 << 13
    csrrs zero, mstatus, t0
    la t0, runtimeInit
    csrw mepc, t0
    la ra, 1f 
    mret

1:
    # Enable only software interrupts
    csrrsi zero, mie, 1 << 3
    csrrsi zero, mstatus, 1 << 3
    # Infinite loop
2:
    wfi
    j 1b

.align 4
# Called for machine mode traps, call out to C code
machineTrapVector:
    csrr a0, mcause
    csrr a1, mepc
    csrr a2, mtval
    csrr a3, mscratch
    la gp, __global_pointer
    la sp, __stack_top
    jal machineTrap
    mret

