
.section .text
.global setSatpCsr
.global addressTranslationFence

# fn (uint64_t satp);
setSatpCsr:
    csrw satp, t0
    ret

# fn (int asid);
addressTranslationFence:
    sfence.vma zero, a0
    ret

