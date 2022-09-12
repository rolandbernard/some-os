
#include <string.h>

#include "kernel/devtree.h"

#include "error/log.h"
#include "error/panic.h"
#include "util/util.h"

#define DEBUG_LOG_SYSCALLS
#ifdef DEBUG_LOG_SYSCALLS
#define DEBUG_DEBTREE(FMT, ...) logKernelMessage(STYLE_DEBUG FMT "\e[m" __VA_OPT__(,) __VA_ARGS__);
#else
#define DEBUG_DEBTREE(...)
#endif

#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE 0x00000002
#define FDT_PROP 0x00000003
#define FDT_NOP 0x00000004
#define FDT_END 0x00000009

// Copy the device tree into an internal format. Then use the functions in this file to query
// the tree after this during initAllSystems and initDevices.
Error initWithDeviceTree(uint8_t* dtb) {
    if (read32be(dtb) != 0xd00dfeed) {
        return someError(EINVAL, "Invalid device tree magic");
    }
    DEBUG_DEBTREE("[dtb] size: %u\n", read32be(dtb + 4));
    DEBUG_DEBTREE("[dtb] version: %u\n", read32be(dtb + 20));
    uint32_t comp_version = read32be(dtb + 24);
    if (comp_version > 17) {
        KERNEL_WARNING("Unsupported device tree format version");
    }
    DEBUG_DEBTREE("[dtb] boot cpu: %u\n", read32be(dtb + 28));
    uint8_t* mem_rsvmap = dtb + read32be(dtb + 16);
    uintptr_t address = 0;
    uintptr_t size = 1;
    for (size_t i = 0; address != 0 || size != 0; i++) {
        address = read64be(mem_rsvmap + 16 * i);
        size = read64be(mem_rsvmap + 16 * i + 8);
        DEBUG_DEBTREE("[dtb] reserved memory: %p (%lu)\n", address, size);
    }
    uint8_t* dt_struct = dtb + read32be(dtb + 8);
    char* dt_strings = (char*)dtb + read32be(dtb + 12);
    uint32_t token = FDT_NOP;
    for (size_t i = 0; token != FDT_END;) {
        token = read32be(dt_struct + i);
        i += 4;
        switch (token) {
            case FDT_BEGIN_NODE:
                DEBUG_DEBTREE("[dtb] node begin: %s\n", dt_struct + i);
                i += (strlen((char*)dt_struct + i) + 4) & ~3;
                break;
            case FDT_END_NODE:
                DEBUG_DEBTREE("[dtb] node end\n");
                break;
            case FDT_PROP: {
                uint32_t len = read32be(dt_struct + i);
                uint32_t nameoff = read32be(dt_struct + i + 4);
                i += 8;
                char value[len + 1];
                memcpy(value, dt_struct + i, len);
                value[len] = 0;
                i += (len + 3) & ~3;
                bool is_print = true;
                for (size_t i = 0; i < len && is_print; i++) {
                    if ((value[i] < 32 || value[i] > 126) && value[i] != 0) {
                        is_print = false;
                    }
                }
                DEBUG_DEBTREE("[dtb] prop: %s =", dt_strings + nameoff);
                if (is_print) {
                    DEBUG_DEBTREE(" %s [", value);
                }
                for (size_t i = 0; i < len; i++) {
                    DEBUG_DEBTREE(" %02hhx", value[i]);
                }
                if (is_print) {
                    DEBUG_DEBTREE(" ]\n");
                } else {
                    DEBUG_DEBTREE("\n");
                }
                break;
            }
        }
    }
    return simpleError(ENOSYS);
}

