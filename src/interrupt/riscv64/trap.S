
.option norvc
.section .text
.global trapInit
.global enterUserMode
.global enterKernelMode
.global waitForInterrupt

# Simple function to wait for interrupt
waitForInterrupt:
    wfi
    ret

# Initialize 
trapInit:
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

# Enter as user at a1 and save registers at a0
enterUserMode:
    # 0b00 << 11 MPP (Previous Protection Mode) is 0 (U mode)
    li t0, 0b11 << 11
    csrrc x0, sstatus, t0
    j kernelTrapReturn

# Enter as superior at a1 and save registers at a0
enterKernelMode:
    # 0b01 << 11 MPP (Previous Protection Mode) is 1 (S mode)
    li t0, 0b11 << 11
    csrrc zero, sstatus, t0
    li t0, 0b01 << 11
    csrrs zero, sstatus, t0
    j kernelTrapReturn

# Return from trap into a1 and save registers at a0
kernelTrapReturn:
    csrw sepc, a1
    csrw sscratch, a0
    ld  x1,   0(a0)
    ld  x2,   8(a0)
    ld  x3,  16(a0)
    ld  x4,  24(a0)
    ld  x5,  32(a0)
    ld  x6,  40(a0)
    ld  x7,  48(a0)
    ld  x8,  56(a0)
    ld  x9,  64(a0)
    ld x11,  80(a0)
    ld x12,  88(a0)
    ld x13,  96(a0)
    ld x14, 104(a0)
    ld x15, 112(a0)
    ld x16, 120(a0)
    ld x17, 128(a0)
    ld x18, 136(a0)
    ld x19, 144(a0)
    ld x20, 152(a0)
    ld x21, 160(a0)
    ld x22, 168(a0)
    ld x23, 176(a0)
    ld x24, 184(a0)
    ld x25, 192(a0)
    ld x26, 200(a0)
    ld x27, 208(a0)
    ld x28, 216(a0)
    ld x29, 224(a0)
    ld x30, 232(a0)
    ld x31, 240(a0)
    ld x10,  72(a0) # x10 == a0 and must therefore be loaded last
    sret

.align 4
# Called on supervisor tarps, call out to C code
kernelTrapVector:
    csrrw x1, sscratch, x1 # Swap scratch (register save location) with x1
    beqz x1, 1f
    # Save user registers
    sd  x2,   8(x1)
    sd  x3,  16(x1)
    sd  x4,  24(x1)
    sd  x5,  32(x1)
    sd  x6,  40(x1)
    sd  x7,  48(x1)
    sd  x8,  56(x1)
    sd  x9,  64(x1)
    sd x10,  72(x1)
    sd x11,  80(x1)
    sd x12,  88(x1)
    sd x13,  96(x1)
    sd x14, 104(x1)
    sd x15, 112(x1)
    sd x16, 120(x1)
    sd x17, 128(x1)
    sd x18, 136(x1)
    sd x19, 144(x1)
    sd x20, 152(x1)
    sd x21, 160(x1)
    sd x22, 168(x1)
    sd x23, 176(x1)
    sd x24, 184(x1)
    sd x25, 192(x1)
    sd x26, 200(x1)
    sd x27, 208(x1)
    sd x28, 216(x1)
    sd x29, 224(x1)
    sd x30, 232(x1)
    sd x31, 240(x1)
    mv a3, x1
    # Load the user register value for x1
    csrrw x1, sscratch, zero
    sd x1, 0(a3) # Save the x1 user value
    j 2f
1:
    mv a3, zero
2:
    csrr a0, scause
    csrr a1, sepc
    csrr a2, stval
.option push
.option norelax
    la gp, __global_pointer
    la sp, __stack_top
.option pop
    jal kernelTrap
    j kernelTrapReturn

