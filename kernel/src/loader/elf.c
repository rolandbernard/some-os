
#include <string.h>

#include "loader/elf.h"

#include "memory/kalloc.h"
#include "memory/pagealloc.h"
#include "memory/pagetable.h"
#include "files/vfs/file.h"
#include "util/util.h"
#include "error/log.h"

#define MAX_PHDRS 128

bool allocatePages(PageTable* table, uintptr_t addr, size_t filesz, size_t memsz, uint32_t flags) {
    int bits = PAGE_ENTRY_USER | PAGE_ENTRY_AD;
    if ((flags & ELF_PROG_EXEC) != 0) {
        bits |= PAGE_ENTRY_EXEC;
    }
    if ((flags & ELF_PROG_READ) != 0) {
        bits |= PAGE_ENTRY_READ;
    }
    if ((flags & ELF_PROG_WRITE) != 0) {
        bits |= PAGE_ENTRY_WRITE;
    }
    if ((bits & 0b1110) == 0) {
        // We need at least some permissions
        bits |= PAGE_ENTRY_READ;
    }
    uintptr_t position = addr & -PAGE_SIZE;
    for (; position < addr + filesz; position += PAGE_SIZE) {
        PageTableEntry* entry = virtToEntry(table, position);
        if (entry != NULL) {
            // Address is already mapped
            entry->bits |= bits; // Make sure permissions are correct
        } else {
            void* page = allocPage();
            if (page == NULL) {
                // Out of memory
                return false;
            } else {
                mapPage(table, position, (uintptr_t)page, bits, 0);
            }
            if (position + PAGE_SIZE > addr + filesz) {
                // Zero out the remaining bytes
                memset(page + filesz % PAGE_SIZE, 0, PAGE_SIZE - (filesz % PAGE_SIZE));
            }
        }
    }
    for (; position < addr + memsz; position += PAGE_SIZE) {
        // All the rest is zero
        PageTableEntry* entry = virtToEntry(table, position);
        if (entry != NULL) {
            // Address is already mapped
            entry->bits |= bits; // Make sure permissions are correct
            if ((entry->bits & PAGE_ENTRY_COPY) != 0) {
                // If this is a copy on write page remove the write bit
                entry->bits &= ~PAGE_ENTRY_WRITE;
            }
        } else {
            if ((bits & PAGE_ENTRY_WRITE) != 0) {
                mapPage(table, position, (uintptr_t)zero_page, (bits & ~PAGE_ENTRY_WRITE) | PAGE_ENTRY_COPY, 0);
            } else {
                mapPage(table, position, (uintptr_t)zero_page, bits, 0);
            }
        }
    }
    return true;
}

static Error loadProgramSegment(PageTable* table, VfsFile* file, ElfProgramHeader* header) {
    if (header->seg_type == ELF_PROG_TYPE_LOAD && header->memsz != 0) {
        if (allocatePages(table, header->vaddr, header->filesz, header->memsz, header->flags)) {
            size_t size = umin(header->memsz, header->filesz);
            size_t read;
            // Using an unsafeVirtPtr here because we might not have write permissions
            CHECKED(vfsFileReadAt(file, NULL, unsafeVirtPtrFor(header->vaddr, table), size, header->off, &read));
            if (read != size) {
                return simpleError(EIO);
            } else {
                return simpleError(SUCCESS);
            }
        } else {
            return simpleError(ENOMEM);
        }
    } else {
        return simpleError(SUCCESS);
    }
}

Error loadProgramFromElfFile(PageTable* table, VfsFile* file, uintptr_t* entry) {
    ElfHeader header;
    size_t read;
    CHECKED(vfsFileReadAt(file, NULL, virtPtrForKernel(&header), sizeof(ElfHeader), 0, &read));
    if (read != sizeof(ElfHeader)) {
        return simpleError(EIO);
    } else if (
        header.magic != ELF_MAGIC
        || header.machine != ELF_MACHINE_RISCV
        || header.obj_type != ELF_TYPE_EXEC
        || header.phnum > MAX_PHDRS
        || header.phentsize != sizeof(ElfProgramHeader)
    ) {
        return simpleError(ENOEXEC);
    } else {
        ElfProgramHeader* prog_headers = kalloc(header.phnum * sizeof(ElfProgramHeader));
        CHECKED(vfsFileReadAt(
            file, NULL, virtPtrForKernel(prog_headers), header.phnum * sizeof(ElfProgramHeader), header.phoff, &read
        ), dealloc(prog_headers));
        if (read != sizeof(ElfProgramHeader) * header.phnum) {
            dealloc(prog_headers);
            return simpleError(EIO);
        } else {
            for (size_t i = 0; i < header.phnum; i++) {
                CHECKED(loadProgramSegment(table, file, &prog_headers[i]), dealloc(prog_headers));
            }
            // Finished loading all segments
            dealloc(prog_headers);
            *entry = header.entry_addr;
            return simpleError(SUCCESS);
        }
    }
}

