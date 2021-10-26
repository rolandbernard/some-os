
#include "util/util.h"

uint64_t umin(uint64_t a, uint64_t b) {
    return a < b ? a : b;
}

uint64_t umax(uint64_t a, uint64_t b) {
    return a > b ? a : b;
}

int64_t smin(int64_t a, int64_t b) {
    return a < b ? a : b;
}

int64_t smax(int64_t a, int64_t b) {
    return a > b ? a : b;
}

void noop() {
    // Do nothing
}

uint64_t hashInt64(uint64_t x) {
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
}

uint32_t hashInt32(uint32_t x) {
    x = (x ^ (x >> 15)) * 0xd168aaad;
    x = (x ^ (x >> 15)) * 0xaf723597;
    x = (x ^ (x >> 15));
    return x;
}

