
.section .text

# fn (uint64_t satp);
.global setSatpCsr
setSatpCsr:
.cfi_startproc
    csrw satp, a0
    ret
.cfi_endproc

.global getSatpCsr
getSatpCsr:
.cfi_startproc
    csrr a0, satp
    ret
.cfi_endproc

# fn (int asid)
.global addressTranslationCompleteFence
addressTranslationCompleteFence:
.cfi_startproc
    sfence.vma zero, zero
    ret
.cfi_endproc

# fn (int asid);
.global addressTranslationFence
addressTranslationFence:
.cfi_startproc
    sfence.vma zero, a0
    ret
.cfi_endproc

# fn (int asid, uintptr_t virt_addr)
.global addressTranslationFenceAt
addressTranslationFenceAt:
.cfi_startproc
    sfence.vma a1, a0
    ret
.cfi_endproc

# fn ();
.global memoryFence
memoryFence:
.cfi_startproc
    fence
    ret
.cfi_endproc

