
.section .text
.global storeConditional

storeConditional:
cas:
    lr.d t0, (a0)
    bne t0, a1, fail
    sc.d t0, a2, (a0)
    bnez t0, cas
    li a0, 1
    ret
fail:
    li a0, 0
    ret

