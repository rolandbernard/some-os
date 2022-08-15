
#include <string.h>
#include <assert.h>

#include "error/panic.h"
#include "memory/pagetable.h"
#include "memory/virtptr.h"

#include "memory/pagealloc.h"
#include "memory/virtmem.h"
#include "error/log.h"

VirtPtr virtPtrForKernel(const void* addr) {
    return virtPtrFor((uintptr_t)addr, kernel_page_table);
}

VirtPtr virtPtrFor(uintptr_t addr, PageTable* table) {
    VirtPtr ret = {
        .address = (uintptr_t)addr,
        .table = table,
        .allow_all = false,
    };
    return ret;
}

VirtPtr unsafeVirtPtrFor(uintptr_t addr, MemorySpace* mem) {
    VirtPtr ret = {
        .address = (uintptr_t)addr,
        .table = mem,
        .allow_all = true,
    };
    return ret;
}

// Parts are segments of the buffer in continuos physical pages
size_t getVirtPtrParts(VirtPtr addr, size_t length, VirtPtrBufferPart* parts, size_t max_parts, bool write) {
    size_t part_index = 0;
    uintptr_t part_position = addr.address;
    uintptr_t next_position = addr.address;
    while (part_position < addr.address + length) {
        next_position = (next_position + PAGE_SIZE) & -PAGE_SIZE;
        uintptr_t phys_start = virtToPhys(addr.table, part_position, write, addr.allow_all);
        if (phys_start == 0) {
            return part_index;
        }
        uintptr_t phys_end = virtToPhys(addr.table, next_position, write, addr.allow_all);
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

void* virtPtrPartsDo(VirtPtr addr, size_t length, VirtPtrPartsDoCallback callback, void* udata, bool write) {
    uintptr_t part_position = addr.address;
    uintptr_t next_position = addr.address;
    while (part_position < addr.address + length) {
        next_position = (next_position + PAGE_SIZE) & -PAGE_SIZE;
        uintptr_t phys_start = virtToPhys(addr.table, part_position, write, addr.allow_all);
        if (phys_start == 0) {
            return udata;
        }
        uintptr_t phys_end = virtToPhys(addr.table, next_position, write, addr.allow_all);
        if (phys_end - phys_start != next_position - part_position || next_position > addr.address + length) {
            VirtPtrBufferPart part;
            part.address = (void*)phys_start;
            part.offset = part_position - addr.address;
            if (next_position > addr.address + length) {
                part.length = addr.address + length - part_position;
            } else {
                part.length = next_position - part_position;
            }
            part_position = next_position;
            udata = callback(part, udata);
        }
    }
    return udata;
}

void memcpyBetweenVirtPtr(VirtPtr dest, VirtPtr src, size_t n) {
    // This does not support overlaps.
    size_t dest_count = getVirtPtrParts(dest, n, NULL, 0, true);
    size_t src_count = getVirtPtrParts(src, n, NULL, 0, false);
    VirtPtrBufferPart dest_parts[dest_count];
    VirtPtrBufferPart src_parts[src_count];
    getVirtPtrParts(dest, n, dest_parts, dest_count, true);
    getVirtPtrParts(src, n, src_parts, src_count, false);
    size_t index = 0;
    size_t dest_index = 0;
    size_t src_index = 0;
    while (index < n && dest_index < dest_count && src_index < src_count) {
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
    size_t count = getVirtPtrParts(dest, n, NULL, 0, true);
    VirtPtrBufferPart parts[count];
    getVirtPtrParts(dest, n, parts, count, true);
    for (size_t i = 0; i < count; i++) {
        memset(parts[i].address, byte, parts[i].length);
    }
}

uint64_t readInt(VirtPtr addr, size_t size) {
    assert(size == 8 || size == 16 || size == 32 || size == 64);
    uintptr_t phys = virtToPhys(addr.table, addr.address, false, addr.allow_all);
    if (phys == 0) {
        return 0;
    } else {
        if (size == 8) {
            return *(uint8_t*)phys;
        } else if (size == 16) {
            return *(uint16_t*)phys;
        } else if (size == 32) {
            return *(uint32_t*)phys;
        } else {
            return *(uint64_t*)phys;
        }
    }
}

uint64_t readIntAt(VirtPtr addr, size_t i, size_t size) {
    addr.address += i * (size / 8);
    return readInt(addr, size);
}

void writeInt(VirtPtr addr, size_t size, uint64_t value) {
    assert(size == 8 || size == 16 || size == 32 || size == 64);
    uintptr_t phys = virtToPhys(addr.table, addr.address, true, addr.allow_all);
    if (phys != 0) {
        if (size == 8) {
            *(uint8_t*)phys = value;
        } else if (size == 16) {
            *(uint16_t*)phys = value;
        } else if (size == 32) {
            *(uint32_t*)phys = value;
        } else {
            *(uint64_t*)phys = value;
        }
    }
}

void writeIntAt(VirtPtr addr, size_t size, size_t i, uint64_t value) {
    addr.address += i * (size / 8);
    return writeInt(addr, size, value);
}

uint64_t readMisaligned(VirtPtr addr, size_t size) {
    uint64_t ret = 0;
    for (size_t i = 0; i < size; i += 8, addr.address++) {
        ret |= readInt(addr, 8) << i;
    }
    return ret;
}

void writeMisaligned(VirtPtr addr, size_t size, uint64_t value) {
    for (size_t i = 0; i < size; i += 8, addr.address++) {
        writeInt(addr, 8, (value >> i) & 0xff);
    }
}

size_t strlenVirtPtr(VirtPtr str) {
    size_t length = 0;
    while (readInt(str, 8) != 0) {
        str.address++;
        length++;
    }
    return length;
}

void strcpyVirtPtr(VirtPtr dest, VirtPtr src) {
    size_t length = strlenVirtPtr(src);
    memcpyBetweenVirtPtr(dest, src, length + 1);
}

VirtPtr pushToVirtPtrStack(VirtPtr sp, void* ptr, size_t size) {
    sp.address -= size;
    memcpyBetweenVirtPtr(sp, virtPtrForKernel(ptr), size);
    return sp;
}

VirtPtr popFromVirtPtrStack(VirtPtr sp, void* ptr, size_t size) {
    memcpyBetweenVirtPtr(virtPtrForKernel(ptr), sp, size);
    sp.address += size;
    return sp;
}

