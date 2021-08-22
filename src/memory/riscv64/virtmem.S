
.option norvc

.section .text
.global setVirtualMemory

# fn (int asid, PageTable* page_table, bool fence);
setVirtualMemory:
    li t0, 8 << 60
    sll t1, a0, 44
    srl t2, a1, 12
    or t0, t0, t1
    or t0, t0, t2
    csrw satp, t0
    beqz a2, 1f
    sfence.vma zero, a0
1:
    ret

