
#include <string.h>

#include "loader/elf.h"

#include "memory/kalloc.h"
#include "memory/pagealloc.h"
#include "memory/pagetable.h"
#include "files/vfs.h"
#include "util/util.h"
#include "error/log.h"

#define MAX_PHDRS 128

typedef struct {
    PageTable* table;
    VfsFile* file;
    ElfFileLoadCallback callback;
    void* udata;
    ElfHeader header;
    ElfProgramHeader* prog_headers;
    size_t ph_index;
    size_t size;
} LoadElfFileRequest;

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

static void loadProgramSegment(LoadElfFileRequest* request);

static void readProgramSegmentCallback(Error error, size_t read, void* udata) {
    LoadElfFileRequest* request = (LoadElfFileRequest*)udata;
    if (isError(error)) {
        dealloc(request->prog_headers);
        request->callback(error, 0, request->udata);
        dealloc(request);
    } else if (read != request->size) {
        dealloc(request->prog_headers);
        request->callback(simpleError(EIO), 0, request->udata);
        dealloc(request);
    } else {
        request->ph_index++;
        loadProgramSegment(request);
    }
}

static void loadProgramSegment(LoadElfFileRequest* request) {
    if (request->ph_index < request->header.phnum) {
        ElfProgramHeader* header = &request->prog_headers[request->ph_index];
        if (header->seg_type != ELF_PROG_TYPE_LOAD || header->memsz == 0) {
            request->ph_index++;
            loadProgramSegment(request);
        } else {
            if (allocatePages(request->table, header->vaddr, header->filesz, header->memsz, header->flags)) {
                request->size = umin(header->memsz, header->filesz);
                // Using an unsafeVirtPtr here because we might not have write permissions
                vfsReadAt(
                    request->file, NULL, unsafeVirtPtrFor(header->vaddr, request->table), request->size,
                    header->off, readProgramSegmentCallback, request
                );
            } else {
                freeMemorySpace(request->table);
                dealloc(request->prog_headers);
                request->callback(simpleError(ENOMEM), 0, request->udata);
                dealloc(request);
            }
        }
    } else {
        // Finished loading all segments
        dealloc(request->prog_headers);
        request->callback(simpleError(SUCCESS), request->header.entry_addr, request->udata);
        dealloc(request);
    }
}

static void readProgramHeadersCallback(Error error, size_t read, void* udata) {
    LoadElfFileRequest* request = (LoadElfFileRequest*)udata;
    if (isError(error)) {
        dealloc(request->prog_headers);
        request->callback(error, 0, request->udata);
        dealloc(request);
    } else if (read != sizeof(ElfProgramHeader) * request->header.phnum) {
        dealloc(request->prog_headers);
        request->callback(simpleError(EIO), 0, request->udata);
        dealloc(request);
    } else {
        request->ph_index = 0;
        loadProgramSegment(request);
    }
}

static void readHeaderCallback(Error error, size_t read, void* udata) {
    LoadElfFileRequest* request = (LoadElfFileRequest*)udata;
    if (isError(error)) {
        request->callback(error, 0, request->udata);
        dealloc(request);
    } else if (read != sizeof(ElfHeader)) {
        request->callback(simpleError(EIO), 0, request->udata);
        dealloc(request);
    } else if (
        request->header.magic != ELF_MAGIC
        || request->header.machine != ELF_MACHINE_RISCV
        || request->header.obj_type != ELF_TYPE_EXEC
        || request->header.phnum > MAX_PHDRS
        || request->header.phentsize != sizeof(ElfProgramHeader)
    ) {
        request->callback(simpleError(ENOEXEC), 0, request->udata);
        dealloc(request);
    } else {
        request->prog_headers = kalloc(request->header.phnum * sizeof(ElfProgramHeader));
        vfsReadAt(
            request->file, NULL, virtPtrForKernel(request->prog_headers),
            request->header.phnum * sizeof(ElfProgramHeader), request->header.phoff,
            readProgramHeadersCallback, request
        );
    }
}

void loadProgramFromElfFile(PageTable* table, VfsFile* file, ElfFileLoadCallback callback, void* udata) {
    LoadElfFileRequest* request = kalloc(sizeof(LoadElfFileRequest));
    request->table = table;
    request->file = file;
    request->callback = callback;
    request->udata = udata;
    vfsReadAt(file, NULL, virtPtrForKernel(&request->header), sizeof(ElfHeader), 0, readHeaderCallback, request);
}

