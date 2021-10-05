
.section .text.boot
.global _start
.global registerHart
.global getHartStack

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
    la ra, 3f
    mret

1:
    # Wait for boot hart
    fence
    li t0, 2
    la t1, boot_state
    lw t1, (t1)
    beq t0, t1, 2f
.rept 16
    nop
.endr
    j 1b
2:
    # Register the existance of this hart
    jal registerHart
    # Enable only software interrupts
    csrrsi zero, mie, 1 << 3
    csrrsi zero, mstatus, 1 << 3
    # Wait for the hart to be woken up
3:
    wfi
    j 3b

registerHart:
    mv s0, ra
    la a0, hart_lock
    call lockSpinLock
    la t0, hart_count
    lw t1, (t0)
    la t2, hart_ids
    sll t3, t1, 2
    add t2, t2, t3
    csrr t3, mhartid
    sw t3, (t2)
    addi t1, t1, 1
    sw t1, (t0)
    la a0, hart_lock
    call unlockSpinLock
    mv ra, s0
    ret

# Determine the stack for the hart. Only used when no specific stack has been allocated.
# Can not be implemented in C, because we can't use the stack yet
getHartStack:
    li t0, 0
    la t1, hart_count
    lw t1, (t1)
1:
    bge t0, t1, 2f
    la t2, hart_ids
    sll t3, t0, 2
    add t2, t2, t3
    lw t2, (t2)
    csrr t3, mhartid
    beq t2, t3, 2f
    addi t0, t0, 1
    j 1b
2:
    # t0 contains index
    la a0, __stack_top
    sll t0, t0, 11 # 2**11 is one 32th of the allocated stack space
    sub a0, a0, t0
    ret

.align 4
# Called for machine mode traps, call out to C code
machineTrapVector:
    la gp, __global_pointer
    jal getHartStack
    mv sp, a0
    csrr a0, mcause
    csrr a1, mepc
    csrr a2, mtval
    csrr a3, mscratch
    jal machineTrap
    mret

