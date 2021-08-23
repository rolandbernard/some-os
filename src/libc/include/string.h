#ifndef _LIBC_STDLIB_H_
#define _LIBC_STDLIB_H_

#include <stddef.h>

void* memset(void* mem, int byte, size_t n);

void* memmove(void* dest, const void* src, size_t n);

void* memcpy(void* dest, const void* src, size_t n);

#endif
