
#include <stdint.h>
#include <stddef.h>

void* memset(void* mem, int byte, size_t n) {
    uint64_t* addr64 = mem;
    for (size_t i = 0; i < n / 8; i++) {
        addr64[i] = 0;
    }
    uint8_t* addr8 = mem + ((n + 7) & -8);
    for (size_t i = 0; i < n % 8; i++) {
        addr8[i] = 0;
    }
    return mem;
}

