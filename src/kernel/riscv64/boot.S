
.option norvc
.section .data

.section .text.boot
.global kernelMain
.global _start

_start:
    # Go into infinite loop with all but one hart
    csrr t0, mhartid
    bnez t0, 1f
    # Disable address translation
    csrw satp, zero

    # Set global and stack pointer
.option push
.option norelax
    la gp, __global_pointer
    la sp, __stack_top
.option pop

    # Enter the kernel in M mode
    li t0, 0b11 << 11
    csrw mstatus, t0
    la t0, kernelMain
    csrw mepc, t0
    la ra, 1f 
    mret

    # Infinite loop
1:
    wfi
    j 1b

