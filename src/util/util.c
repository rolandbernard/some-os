
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

