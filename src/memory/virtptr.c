
#include <string.h>

#include "memory/virtptr.h"

#include "memory/pagealloc.h"
#include "memory/virtmem.h"
#include "error/log.h"

VirtPtr virtPtrForKernel(void* addr) {
    return virtPtrFor((uintptr_t)addr, kernel_page_table);
}

VirtPtr virtPtrFor(uintptr_t addr, PageTable* table) {
    VirtPtr ret = {
        .address = (uintptr_t)addr,
        .table = table,
    };
    return ret;
}

// Parts are segments of the buffer in continuos physical pages
size_t getVirtPtrParts(VirtPtr addr, size_t length, VirtPtrBufferPart* parts, size_t max_parts) {
    size_t part_index = 0;
    uintptr_t part_position = addr.address;
    uintptr_t next_position = addr.address;
    while (part_position < addr.address + length) {
        next_position = (next_position + PAGE_SIZE) & -PAGE_SIZE;
        uintptr_t phys_start = virtToPhys(addr.table, part_position);
        uintptr_t phys_end = virtToPhys(addr.table, next_position);
        if (phys_end - phys_start != next_position - part_position || next_position > addr.address + length) {
            if (part_index < max_parts) {
                parts[part_index].address = (void*)phys_start;
                parts[part_index].offset = part_position - addr.address;
                if (next_position > addr.address + length) {
                    parts[part_index].length = addr.address + length - part_position;
                } else {
                    parts[part_index].length = next_position - part_position;
                }
            }
            part_position = next_position;
            part_index++;
        }
    }
    return part_index;
}

void memcpyBetweenVirtPtr(VirtPtr dest, VirtPtr src, size_t n) {
    // This does not support overlaps.
    size_t dest_count = getVirtPtrParts(dest, n, NULL, 0);
    size_t src_count = getVirtPtrParts(dest, n, NULL, 0);
    VirtPtrBufferPart dest_parts[dest_count];
    VirtPtrBufferPart src_parts[src_count];
    getVirtPtrParts(dest, n, dest_parts, dest_count);
    getVirtPtrParts(dest, n, src_parts, src_count);
    size_t index = 0;
    size_t dest_index = 0;
    size_t src_index = 0;
    while (index < n) {
        size_t dest_next = dest_parts[dest_index].offset + dest_parts[dest_index].length;
        size_t src_next = src_parts[src_index].offset + src_parts[src_index].length;
        size_t dest_off = index - dest_parts[dest_index].offset;
        size_t src_off = index - src_parts[src_index].offset;
        if (dest_next < src_next) {
            memmove(dest_parts[dest_index].address + dest_off, src_parts[src_index].address + src_off, dest_next - index);
            index = dest_next;
            dest_index++;
        } else {
            memmove(dest_parts[dest_index].address + dest_off, src_parts[src_index].address + src_off, src_next - index);
            index = src_next;
            src_index++;
        }
    }
}

void memsetVirtPtr(VirtPtr dest, int byte, size_t n) {
    size_t count = getVirtPtrParts(dest, n, NULL, 0);
    VirtPtrBufferPart parts[count];
    getVirtPtrParts(dest, n, parts, count);
    for (size_t i = 0; i < count; i++) {
        memset(parts[i].address, byte, parts[i].length);
    }
}

