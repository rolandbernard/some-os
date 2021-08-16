
.option norvc
.section .data

.section .text
.global trapInit

trapInit:
    la t0, machineTrapVector
    csrw mtvec, t0
    la t0, kernelTrapVector
    csrw stvec, t0
    ret

machineTrapReturn:
    mret

.align 4
machineTrapVector:
    csrr a0, mcause
    csrr a1, mepc
    csrr a2, mtval
    csrr a3, mscratch
    jal kernelTrap
    j machineTrapReturn


kernelTrapReturn:
    sret

.align 4
kernelTrapVector:
    jal kernelTrap
    j kernelTrapReturn

