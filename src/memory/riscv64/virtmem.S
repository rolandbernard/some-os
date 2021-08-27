
.section .text
.global setVirtualMemory

# fn (int asid, PageTable* page_table, bool fence);
setVirtualMemory:
    bnez a1, 1f
    csrw satp, zero
    j 2f
1:
    li t0, 8 << 60
    sll t1, a0, 44
    srl t2, a1, 12
    or t0, t0, t1
    or t0, t0, t2
    csrw satp, t0
    beqz a2, 2f
    sfence.vma zero, a0
2:
    ret

