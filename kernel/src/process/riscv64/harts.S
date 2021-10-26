
.section .text
.global readMhartid
.global readSscratch
.global writeSscratch

readMhartid:
    csrr a0, mhartid
    ret

readSscratch:
    csrr a0, sscratch
    ret

writeSscratch:
    csrw sscratch, a0
    ret

