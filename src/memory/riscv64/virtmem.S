
.section .text
.global setSatpCsr
.global addressTranslationFence
.global memoryFence

# fn (uint64_t satp);
setSatpCsr:
    csrw satp, a0
    ret

# fn (int asid);
addressTranslationFence:
    sfence.vma zero, a0
    ret

# fn ();
memoryFence:
    fence
    ret

