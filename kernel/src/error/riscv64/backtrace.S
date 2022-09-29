
.altmacro

.section .text

.macro cfi_gp i
.cfi_offset x\i, ((\i)*8)
.endm

.global magicCfiIndirect
magicCfiIndirect:
.cfi_startproc
.set i, 1
.rept 31
    cfi_gp %i
    .set i, i+1
.endr
.cfi_offset x0, 512
.cfi_return_column x0
    mv s0, a0
.cfi_def_cfa s0, 0
    mv s1, ra
    jalr a1
    jr s1
.cfi_endproc

