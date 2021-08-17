
.option norvc

.section .text.boot
.global _start

_start:
    # Go into infinite loop with all but one hart
    csrr t0, mhartid
    bnez t0, 1f

    # Set global and stack pointer
.option push
.option norelax
    la gp, __global_pointer
    la sp, __stack_top
.option pop

    # Initialize trap data
    jal trapInit

    # Disable address translation
    csrw satp, zero
    # Give S and U mode access to memory
    csrw pmpcfg0, 0xF
    li t0, -1
    csrw pmpaddr0, t0

    # Enter the kernel in S mode
    li t0, 0b11 << 11
    csrrc zero, mstatus, t0
    li t0, 0b01 << 11
    csrrs zero, mstatus, t0
    la t0, runtimeInit
    csrw mepc, t0
    la ra, 1f 
    mret

    # Infinite loop
1:
    wfi
    j 1b

