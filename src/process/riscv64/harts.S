
.section .text
.global readSscratch
.global writeSscratch

readSscratch:
    csrr a0, sscratch
    ret

writeSscratch:
    csrw sscratch, a0
    ret

