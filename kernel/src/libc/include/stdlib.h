#ifndef _LIBC_STDLIB_H_
#define _LIBC_STDLIB_H_

#include <stdnoreturn.h>
#include <stddef.h>

noreturn void abort();

void* malloc(size_t size);

void free(void* ptr);

#endif
