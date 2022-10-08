
#include <string.h>

#include "interrupt/timer.h"
#include "util/util.h"

#include "util/random.h"

typedef uint32_t Bits256[8];
typedef uint32_t Bits128[4];

static uint32_t rotateRight(uint32_t v, uint32_t r) {
    return (v >> r) | (v << (32 - r));
}

static void sha256Init(Bits256 hash) {
    hash[0] = 0x6a09e667;
    hash[1] = 0xbb67ae85;
    hash[2] = 0x3c6ef372;
    hash[3] = 0xa54ff53a;
    hash[4] = 0x510e527f;
    hash[5] = 0x9b05688c;
    hash[6] = 0x1f83d9ab;
    hash[7] = 0x5be0cd19;
}

static void sha256Block(Bits256 hash, Bits256 block) {
    static const uint32_t constants[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
        0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
        0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
        0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
        0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
        0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
        0xc67178f2
    };
    uint32_t schedule[64];
    memcpy(schedule, block, sizeof(Bits256));
    for (size_t i = 16; i < 64; i++) {
        uint32_t s0 = rotateRight(schedule[i - 15], 7) ^ rotateRight(schedule[i - 15], 18) ^ rotateRight(schedule[i - 15], 3);
        uint32_t s1 = rotateRight(schedule[i - 2], 17) ^ rotateRight(schedule[i - 2], 19) ^ rotateRight(schedule[i - 2], 10);
        schedule[i] = schedule[i - 16] + s0 + schedule[i - 7] + s1;
    }
    uint32_t a = hash[0];
    uint32_t b = hash[1];
    uint32_t c = hash[2];
    uint32_t d = hash[3];
    uint32_t e = hash[4];
    uint32_t f = hash[5];
    uint32_t g = hash[6];
    uint32_t h = hash[7];
    for (size_t i = 0; i < 64; i++) {
        uint32_t S1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + S1 + ch + constants[i] + schedule[i];
        uint32_t S0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    hash[0] += a;
    hash[1] += b;
    hash[2] += c;
    hash[3] += d;
    hash[4] += e;
    hash[5] += f;
    hash[6] += g;
    hash[7] += h;
}

static void aesBlock(Bits256 key, Bits128 block) {
    // static const uint8_t lookup[256] = {
    //     0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab,
    //     0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4,
    //     0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71,
    //     0xd8, 0x31, 0x15, 0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2,
    //     0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6,
    //     0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb,
    //     0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf, 0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45,
    //     0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
    //     0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44,
    //     0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73, 0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a,
    //     0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49,
    //     0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d,
    //     0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08, 0xba, 0x78, 0x25,
    //     0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e,
    //     0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1,
    //     0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    //     0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb,
    //     0x16
    // };
    // Bits128 round_keys[15];
}

#define MIN_RESEED_SIZE 32
#define ENTROPY_POOLS 32

Bits256 pools[ENTROPY_POOLS];
size_t pools_size;
size_t acc_count;
size_t req_count;

Bits256 key;
Bits128 counter;
Time last_reseed;

void addRandomEvent(uint8_t* data, size_t size) {
    Bits256 block;
    block[0] = getTime() >> 32;
    block[1] = getTime();
    block[2] = size;
    memcpy(block + 3, data, size);
    sha256Block(pools[acc_count % ENTROPY_POOLS], block);
    acc_count++;
    if (acc_count % ENTROPY_POOLS == 0) {
        pools_size++;
    }
}

static void reseedRandom() {
    if (last_reseed == 0) {
        sha256Init(key);
    }
    for (size_t i = 0; i < ENTROPY_POOLS && req_count % (1 << i) == 0; i++) {
        sha256Block(key, pools[i]);
    }
    req_count++;
    pools_size = 0;
    last_reseed = getTime();
}

static void advanceCounter() {
    uint32_t carry = 1;
    for (size_t i = 0; carry != 0 && i < sizeof(Bits128) / sizeof(uint32_t); i++) {
        uint64_t next = carry + counter[i];
        counter[i] = next;
        carry = next >> 32;
    }
}

static void randomBlock(Bits128 block) {
    memcpy(block, counter, sizeof(Bits128));
    aesBlock(key, block);
    advanceCounter();
}

void getRandom(VirtPtr bytes, size_t size) {
    Time now = getTime();
    if (last_reseed == 0 || (last_reseed + CLOCKS_PER_SEC / 10 < now && pools_size >= MIN_RESEED_SIZE)) {
        reseedRandom();
    }
    while (size > 0) {
        size_t left = umin(size, 1 << 20);
        size -= left;
        while (left > 0) {
            Bits128 block;
            randomBlock(block);
            size_t this = umin(left, sizeof(Bits128));
            memcpyBetweenVirtPtr(bytes, virtPtrForKernel(block), this);
            left -= this;
            bytes.address += this;
        }
        Bits256 new_key;
        randomBlock(new_key);
        randomBlock(new_key + 1);
        memcpy(key, new_key, sizeof(Bits256));
    }
}

