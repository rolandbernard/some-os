
#include <stdint.h>
#include <stddef.h>

void* memset(void* mem, int byte, size_t n) {
    while ((uintptr_t)mem % 8 != 0) {
        *((uint8_t*)mem) = byte;
        mem++;
        n--;
    }
    uint64_t big = 0;
    for (int i = 0; i < 8; i++) {
        big |= ((uint64_t)byte & 0xff) << (8 * i);
    }
    while (n > 8) {
        *((uint64_t*)mem) = big;
        mem += 8;
        n -= 8;
    }
    while (n > 0) {
        *((uint8_t*)mem) = byte;
        mem++;
        n--;
    }
    return mem;
}

void* memmove(void* dest, const void* src, size_t n) {
    if (dest < src) {
        while ((uintptr_t)dest % 8 != 0 && (uintptr_t)src % 8 != 0) {
            (*(uint8_t*)dest) = *(uint8_t*)src;
            dest++;
            src++;
            n--;
        }
        while (n > 8) {
            (*(uint64_t*)dest) = *(uint64_t*)src;
            dest += 8;
            src += 8;
            n -= 8;
        }
        while (n > 0) {
            (*(uint8_t*)dest) = *(uint8_t*)src;
            dest++;
            src++;
            n--;
        }
    } else {
        dest += n;
        src += n;
        while ((uintptr_t)dest % 8 != 0 && (uintptr_t)src % 8 != 0) {
            dest--;
            src--;
            (*(uint8_t*)dest) = *(uint8_t*)src;
            n--;
        }
        while (n > 8) {
            dest -= 8;
            src -= 8;
            (*(uint64_t*)dest) = *(uint64_t*)src;
            n -= 8;
        }
        while (n > 0) {
            dest--;
            src--;
            (*(uint8_t*)dest) = *(uint8_t*)src;
            n--;
        }
    }
    return dest;
}

void* memcpy(void* dest, const void* src, size_t n) {
    // No need to optimize for now
    return memmove(dest, src, n);
}

