
.section .text

# fn (uint64_t satp);
.global setSatpCsr
setSatpCsr:
    csrw satp, a0
    ret

.global getSatpCsr
getSatpCsr:
    csrr a0, satp
    ret

# fn (int asid)
.global addressTranslationCompleteFence
addressTranslationCompleteFence:
    sfence.vma zero, zero
    ret

# fn (int asid);
.global addressTranslationFence
addressTranslationFence:
    sfence.vma zero, a0
    ret

# fn (int asid, uintptr_t virt_addr)
.global addressTranslationFenceAt
addressTranslationFenceAt:
    sfence.vma a1, a0
    ret

# fn ();
.global memoryFence
memoryFence:
    fence
    ret

