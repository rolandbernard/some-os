
.section .text
.global setSatpCsr
.global getSatpCsr
.global addressTranslationCompleteFence
.global addressTranslationFence
.global addressTranslationFenceAt
.global memoryFence

# fn (uint64_t satp);
setSatpCsr:
    csrw satp, a0
    ret

getSatpCsr:
    csrr a0, satp
    ret

# fn (int asid);
addressTranslationCompleteFence:
    sfence.vma zero, zero
    ret

# fn (int asid);
addressTranslationFence:
    sfence.vma zero, a0
    ret

# fn (int asid, uintptr_t virt_addr)
addressTranslationFenceAt:
    sfence.vma a1, a0
    ret

# fn ();
memoryFence:
    fence
    ret

