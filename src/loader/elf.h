#ifndef _ELF_H_
#define _ELF_H_

#include <stdint.h>

#include "error/error.h"
#include "files/vfs.h"
#include "process/types.h"
#include "memory/virtptr.h"

// This is in little endian: 0x7f byte followed by "ELF"
#define ELF_MAGIC 0x464c457f
#define ELF_TYPE_EXEC 2
#define ELF_MACHINE_RISCV 0xf3

typedef struct {
    uint32_t magic;
    uint8_t bitsize;
    uint8_t endian;
    uint8_t ident_iba_version;
    uint8_t target_platform;
    uint8_t abi_version;
    uint8_t padding[7];
    uint16_t obj_type;
    uint16_t machine;
    uint32_t version;
    uintptr_t entry_addr;
    uintptr_t phoff;
    uintptr_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} ElfHeader;

#define ELF_PROG_READ 4
#define ELF_PROG_WRITE 2
#define ELF_PROG_EXEC 1

#define ELF_PROG_TYPE_NULL 0
#define ELF_PROG_TYPE_LOAD 1
#define ELF_PROG_TYPE_DYNAMIC 2
#define ELF_PROG_TYPE_INTERP 3
#define ELF_PROG_TYPE_NOTE 4

typedef struct {
    uint32_t seg_type;
    uint32_t flags;
    uintptr_t off;
    uintptr_t vaddr;
    uintptr_t paddr;
    uintptr_t filesz;
    uintptr_t memsz;
    uintptr_t align;
} ElfProgramHeader;

typedef void (*ElfFileLoadCallback)(Error error, void* udata);

void loadProgramFromElfFile(Process* process, VfsFile* file, ElfFileLoadCallback callback, void* udata);

#endif
