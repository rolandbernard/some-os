
#include <assert.h>
#include <string.h>

#include "error/panic.h"
#include "interrupt/timer.h"
#include "task/spinlock.h"
#include "util/util.h"

#include "util/random.h"

typedef uint32_t Bits256[8];
typedef uint32_t Bits128[4];

static uint32_t rotateRight(uint32_t v, uint32_t r) {
    return (v >> r) | (v << (32 - r));
}

static uint32_t rotateLeft(uint32_t v, uint32_t r) {
    return (v << r) | (v >> (32 - r));
}

static uint32_t subWord(uint32_t v) {
    static const uint8_t sbox[256] = {
        0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab,
        0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4,
        0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71,
        0xd8, 0x31, 0x15, 0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2,
        0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6,
        0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb,
        0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf, 0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45,
        0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
        0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44,
        0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73, 0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a,
        0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49,
        0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d,
        0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08, 0xba, 0x78, 0x25,
        0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e,
        0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1,
        0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
        0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb,
        0x16
    };
    return sbox[v & 0xff] | (sbox[(v >> 8) & 0xff] << 8)
        | (sbox[(v >> 16) & 0xff] << 16) | (sbox[(v >> 24) & 0xff] << 24);
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

static void aesKeyExpansion(Bits256 key, Bits128 round_keys[15]) {
    static const uint8_t constants[11] = {
        0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
    };
    memcpy(round_keys[0], key, sizeof(Bits128));
    memcpy(round_keys[1], key + 4, sizeof(Bits128));
    uint32_t* flat_keys = (uint32_t*)round_keys;
    for (size_t i = 8; i < 4 * 15; i++) {
        uint32_t tmp = flat_keys[i - 1];
        if (i % 8 == 0) {
            tmp = subWord(rotateLeft(tmp, 8)) ^ constants[i / 8];
        } else if (i % 8 == 4) {
            tmp = subWord(tmp);
        }
        flat_keys[i] = flat_keys[i - 8] ^ tmp;
    }
}

static void aesAddRoundKey(Bits128 block, Bits128 key) {
    for (size_t i = 0; i < 4; i++) {
        block[i] ^= key[i];
    }
}

static void aesSubBytes(Bits128 block) {
    for (size_t i = 0; i < 4; i++) {
        block[i] = subWord(block[i]);
    }
}

static void aesShiftRows(Bits128 block) {
    for (size_t i = 1; i < 4; i++) {
        uint32_t mask = 0xff << (i * 8);
        for (size_t s = 0; s < i; s++) {
            uint32_t tmp = block[0];
            for (size_t j = 0; j < 3; j++) {
                block[j] = (block[j] & ~mask) | (block[j + 1] & mask);
            }
            block[3] = (block[3] & ~mask) | (tmp & mask);
        }
    }
}

static uint8_t mulBytes(uint8_t a, uint8_t b) {
    static const uint8_t gmul2[256] = {
        0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1a, 0x1c,
        0x1e, 0x20, 0x22, 0x24, 0x26, 0x28, 0x2a, 0x2c, 0x2e, 0x30, 0x32, 0x34, 0x36, 0x38, 0x3a,
        0x3c, 0x3e, 0x40, 0x42, 0x44, 0x46, 0x48, 0x4a, 0x4c, 0x4e, 0x50, 0x52, 0x54, 0x56, 0x58,
        0x5a, 0x5c, 0x5e, 0x60, 0x62, 0x64, 0x66, 0x68, 0x6a, 0x6c, 0x6e, 0x70, 0x72, 0x74, 0x76,
        0x78, 0x7a, 0x7c, 0x7e, 0x80, 0x82, 0x84, 0x86, 0x88, 0x8a, 0x8c, 0x8e, 0x90, 0x92, 0x94,
        0x96, 0x98, 0x9a, 0x9c, 0x9e, 0xa0, 0xa2, 0xa4, 0xa6, 0xa8, 0xaa, 0xac, 0xae, 0xb0, 0xb2,
        0xb4, 0xb6, 0xb8, 0xba, 0xbc, 0xbe, 0xc0, 0xc2, 0xc4, 0xc6, 0xc8, 0xca, 0xcc, 0xce, 0xd0,
        0xd2, 0xd4, 0xd6, 0xd8, 0xda, 0xdc, 0xde, 0xe0, 0xe2, 0xe4, 0xe6, 0xe8, 0xea, 0xec, 0xee,
        0xf0, 0xf2, 0xf4, 0xf6, 0xf8, 0xfa, 0xfc, 0xfe, 0x1b, 0x19, 0x1f, 0x1d, 0x13, 0x11, 0x17,
        0x15, 0x0b, 0x09, 0x0f, 0x0d, 0x03, 0x01, 0x07, 0x05, 0x3b, 0x39, 0x3f, 0x3d, 0x33, 0x31,
        0x37, 0x35, 0x2b, 0x29, 0x2f, 0x2d, 0x23, 0x21, 0x27, 0x25, 0x5b, 0x59, 0x5f, 0x5d, 0x53,
        0x51, 0x57, 0x55, 0x4b, 0x49, 0x4f, 0x4d, 0x43, 0x41, 0x47, 0x45, 0x7b, 0x79, 0x7f, 0x7d,
        0x73, 0x71, 0x77, 0x75, 0x6b, 0x69, 0x6f, 0x6d, 0x63, 0x61, 0x67, 0x65, 0x9b, 0x99, 0x9f,
        0x9d, 0x93, 0x91, 0x97, 0x95, 0x8b, 0x89, 0x8f, 0x8d, 0x83, 0x81, 0x87, 0x85, 0xbb, 0xb9,
        0xbf, 0xbd, 0xb3, 0xb1, 0xb7, 0xb5, 0xab, 0xa9, 0xaf, 0xad, 0xa3, 0xa1, 0xa7, 0xa5, 0xdb,
        0xd9, 0xdf, 0xdd, 0xd3, 0xd1, 0xd7, 0xd5, 0xcb, 0xc9, 0xcf, 0xcd, 0xc3, 0xc1, 0xc7, 0xc5,
        0xfb, 0xf9, 0xff, 0xfd, 0xf3, 0xf1, 0xf7, 0xf5, 0xeb, 0xe9, 0xef, 0xed, 0xe3, 0xe1, 0xe7,
        0xe5
    };
    static const uint8_t gmul3[256] = {
        0x00, 0x03, 0x06, 0x05, 0x0c, 0x0f, 0x0a, 0x09, 0x18, 0x1b, 0x1e, 0x1d, 0x14, 0x17, 0x12,
        0x11, 0x30, 0x33, 0x36, 0x35, 0x3c, 0x3f, 0x3a, 0x39, 0x28, 0x2b, 0x2e, 0x2d, 0x24, 0x27,
        0x22, 0x21, 0x60, 0x63, 0x66, 0x65, 0x6c, 0x6f, 0x6a, 0x69, 0x78, 0x7b, 0x7e, 0x7d, 0x74,
        0x77, 0x72, 0x71, 0x50, 0x53, 0x56, 0x55, 0x5c, 0x5f, 0x5a, 0x59, 0x48, 0x4b, 0x4e, 0x4d,
        0x44, 0x47, 0x42, 0x41, 0xc0, 0xc3, 0xc6, 0xc5, 0xcc, 0xcf, 0xca, 0xc9, 0xd8, 0xdb, 0xde,
        0xdd, 0xd4, 0xd7, 0xd2, 0xd1, 0xf0, 0xf3, 0xf6, 0xf5, 0xfc, 0xff, 0xfa, 0xf9, 0xe8, 0xeb,
        0xee, 0xed, 0xe4, 0xe7, 0xe2, 0xe1, 0xa0, 0xa3, 0xa6, 0xa5, 0xac, 0xaf, 0xaa, 0xa9, 0xb8,
        0xbb, 0xbe, 0xbd, 0xb4, 0xb7, 0xb2, 0xb1, 0x90, 0x93, 0x96, 0x95, 0x9c, 0x9f, 0x9a, 0x99,
        0x88, 0x8b, 0x8e, 0x8d, 0x84, 0x87, 0x82, 0x81, 0x9b, 0x98, 0x9d, 0x9e, 0x97, 0x94, 0x91,
        0x92, 0x83, 0x80, 0x85, 0x86, 0x8f, 0x8c, 0x89, 0x8a, 0xab, 0xa8, 0xad, 0xae, 0xa7, 0xa4,
        0xa1, 0xa2, 0xb3, 0xb0, 0xb5, 0xb6, 0xbf, 0xbc, 0xb9, 0xba, 0xfb, 0xf8, 0xfd, 0xfe, 0xf7,
        0xf4, 0xf1, 0xf2, 0xe3, 0xe0, 0xe5, 0xe6, 0xef, 0xec, 0xe9, 0xea, 0xcb, 0xc8, 0xcd, 0xce,
        0xc7, 0xc4, 0xc1, 0xc2, 0xd3, 0xd0, 0xd5, 0xd6, 0xdf, 0xdc, 0xd9, 0xda, 0x5b, 0x58, 0x5d,
        0x5e, 0x57, 0x54, 0x51, 0x52, 0x43, 0x40, 0x45, 0x46, 0x4f, 0x4c, 0x49, 0x4a, 0x6b, 0x68,
        0x6d, 0x6e, 0x67, 0x64, 0x61, 0x62, 0x73, 0x70, 0x75, 0x76, 0x7f, 0x7c, 0x79, 0x7a, 0x3b,
        0x38, 0x3d, 0x3e, 0x37, 0x34, 0x31, 0x32, 0x23, 0x20, 0x25, 0x26, 0x2f, 0x2c, 0x29, 0x2a,
        0x0b, 0x08, 0x0d, 0x0e, 0x07, 0x04, 0x01, 0x02, 0x13, 0x10, 0x15, 0x16, 0x1f, 0x1c, 0x19,
        0x1a
    };
    if (a == 2) {
        return gmul2[b];
    } else if (a == 3) {
        return gmul3[b];
    } else {
        panic();
    }
}

static uint32_t mixColumn(uint32_t v) {
    uint8_t a[4] = {
        v & 0xff,
        (v >> 8) & 0xff,
        (v >> 16) & 0xff,
        (v >> 24) & 0xff,
    };
    return (mulBytes(0x02, a[0]) ^ mulBytes(0x03, a[1]) ^ a[2] ^ a[3])
           | ((a[0] ^ mulBytes(0x02, a[1]) ^ mulBytes(0x03, a[2]) ^ a[3]) << 8)
           | ((a[0] ^ a[1] ^ mulBytes(0x02, a[2]) ^ mulBytes(0x03, a[3])) << 16)
           | ((mulBytes(0x03, a[0]) ^ a[1] ^ a[2] ^ mulBytes(0x02, a[3])) << 24);
}

static void aesMixColumns(Bits128 block) {
    for (size_t i = 0; i < 4; i++) {
        block[i] = mixColumn(block[i]);
    }
}

static void aesBlock(Bits256 key, Bits128 block) {
    Bits128 round_keys[15];
    aesKeyExpansion(key, round_keys);
    aesAddRoundKey(block, round_keys[0]);
    for (size_t i = 0; i < 13; i++) {
        aesSubBytes(block);
        aesShiftRows(block);
        aesMixColumns(block);
        aesAddRoundKey(block, round_keys[i + 1]);
    }
    aesSubBytes(block);
    aesShiftRows(block);
    aesAddRoundKey(block, round_keys[14]);
}

#define MIN_RESEED_SIZE 32
#define ENTROPY_POOLS 32

static Bits256 pools[ENTROPY_POOLS];
static size_t pools_size;
static size_t acc_count;
static size_t req_count;

static Bits256 key;
static Bits128 counter;
static Time last_reseed;

static SpinLock lock;

void addRandomEvent(uint8_t* data, size_t size) {
    assert(size < sizeof(Bits256) - 2 * sizeof(uint32_t));
    lockSpinLock(&lock);
    if (acc_count == 0) {
        for (size_t i = 0; i < ENTROPY_POOLS; i++) {
            sha256Init(pools[i]);
        }
        pools_size = 0;
    }
    Bits256 block;
    block[0] = getTime();
    block[1] = size;
    memcpy(block + 2, data, size);
    sha256Block(pools[acc_count % ENTROPY_POOLS], block);
    acc_count++;
    if (acc_count % ENTROPY_POOLS == 0) {
        pools_size++;
    }
    unlockSpinLock(&lock);
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
    lockSpinLock(&lock);
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
        randomBlock(new_key + 4);
        memcpy(key, new_key, sizeof(Bits256));
    }
    unlockSpinLock(&lock);
}
