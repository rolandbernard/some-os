
#include <stdlib.h>

#include "error/panic.h"
#include "memory/kalloc.h"

noreturn void abort() {
    KERNEL_LOG("[!] Kernel abort!")
    notifyPanic();
}

void* malloc(size_t size) {
    return kalloc(size);
}

void free(void* ptr) {
    dealloc(ptr);
}

