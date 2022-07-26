#ifndef _LIBC_STRING_H_
#define _LIBC_STRING_H_

#include <stddef.h>

void* memset(void* mem, int byte, size_t n);

void* memmove(void* dest, const void* src, size_t n);

void* memcpy(void* dest, const void* src, size_t n);

int strcmp(const char* s1, const char* s2);

int strncmp(const char* s1, const char* s2, size_t n);

size_t strlen(const char* s);

#endif
