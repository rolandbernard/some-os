
#include <string.h>

#include "kernel/devtree.h"

#include "error/log.h"
#include "error/panic.h"
#include "memory/kalloc.h"
#include "util/util.h"

#define DEBUG_LOG_DEVTREE
#ifdef DEBUG_LOG_DEVTREE
#define DEBUG_DEVTREE(FMT, ...) logKernelMessage(STYLE_DEBUG FMT "\e[m" __VA_OPT__(,) __VA_ARGS__);
#else
#define DEBUG_DEVTREE(...)
#endif

#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE 0x00000002
#define FDT_PROP 0x00000003
#define FDT_NOP 0x00000004
#define FDT_END 0x00000009

typedef struct {
    uintptr_t addr;
    uintptr_t size;
} ReservedMemory;

typedef struct {
    char* name;
    size_t len;
    uint8_t* value;
} DeviceTreeProperty;

typedef struct DeviceTreeNode_s {
    char* name;
    size_t prop_count;
    DeviceTreeProperty* props;
    size_t node_count;
    struct DeviceTreeNode_s* nodes;
} DeviceTreeNode;

typedef struct {
    uint8_t* raw;
    size_t reserved_count;
    ReservedMemory* reserved_memory;
    DeviceTreeNode root;
} DeviceTree;

static DeviceTree device_tree;

static Error parseReservedMemoryRegions(uint8_t* mem_rsvmap) {
    device_tree.reserved_count = 0;
    uintptr_t address;
    uintptr_t size;
    do {
        address = read64be(mem_rsvmap);
        size = read64be(mem_rsvmap + 8);
        mem_rsvmap += 16;
        if (address != 0 || size != 0) {
            device_tree.reserved_count++;
            device_tree.reserved_memory = krealloc(
                device_tree.reserved_memory, device_tree.reserved_count * sizeof(ReservedMemory)
            );
            ReservedMemory* resv_mem = &device_tree.reserved_memory[device_tree.reserved_count - 1];
            resv_mem->addr = address;
            resv_mem->size = size;
            DEBUG_DEVTREE("[dtb] reserved memory: %p-%p (%lu)\n", address, address + size, size);
        }
    } while (address != 0 || size != 0);
    return simpleError(SUCCESS);
}

static uint8_t* parseDeviceTreeNode(uint8_t* dt_struct, char* dt_strings, DeviceTreeNode* node, size_t depth) {
#ifdef DEBUG_LOG_DEVTREE
    char indent[2 * depth + 1];
    memset(indent, ' ', 2 * depth);
    indent[2 * depth] = 0;
#endif
    node->prop_count = 0;
    node->props = NULL;
    node->node_count = 0;
    node->nodes = NULL;
    uint32_t token = FDT_NOP;
    while (token != FDT_END && token != FDT_END_NODE) {
        token = read32be(dt_struct);
        dt_struct += 4;
        if (token == FDT_BEGIN_NODE) {
            DEBUG_DEVTREE("[dtb] %s node %s\n", indent, dt_struct);
            node->node_count++;
            node->nodes = krealloc(node->nodes, node->node_count * sizeof(DeviceTreeNode));
            DeviceTreeNode* child = &node->nodes[node->node_count - 1];
            child->name = (char*)dt_struct;
            dt_struct += (strlen((char*)dt_struct) + 4) & ~3;
            dt_struct = parseDeviceTreeNode(dt_struct, dt_strings, child, depth + 1);
        } else if (token == FDT_PROP) {
            uint32_t len = read32be(dt_struct);
            uint32_t nameoff = read32be(dt_struct + 4);
            dt_struct += 8;
            node->prop_count++;
            node->props = krealloc(node->props, node->prop_count * sizeof(DeviceTreeProperty));
            DeviceTreeProperty* prop = &node->props[node->prop_count - 1];
            prop->name = dt_strings + nameoff;
            prop->len = len;
            prop->value = dt_struct;
            dt_struct += (len + 3) & ~3;
#ifdef DEBUG_LOG_DEVTREE
            bool is_print = true;
            for (size_t i = 0; i < len && is_print; i++) {
                if ((prop->value[i] < 32 || prop->value[i] > 126) && prop->value[i] != 0) {
                    is_print = false;
                }
            }
            DEBUG_DEVTREE("[dtb] %s prop %s =", indent, prop->name);
            if (is_print) {
                char value[len + 1];
                memcpy(value, prop->value, len);
                value[len] = 0;
                DEBUG_DEVTREE(" %s [", value);
            }
            for (size_t i = 0; i < len; i++) {
                DEBUG_DEVTREE(" %02hhx", prop->value[i]);
            }
            if (is_print) {
                DEBUG_DEVTREE(" ]\n");
            } else {
                DEBUG_DEVTREE("\n");
            }
#endif
        }
    }
    return dt_struct;
}

static Error parseDeviceTreeNodes(uint8_t* dt_struct, char* dt_strings) {
    device_tree.root.name = "";
    DEBUG_DEVTREE("[dtb] root\n");
    parseDeviceTreeNode(dt_struct, dt_strings, &device_tree.root, 0);
    return simpleError(SUCCESS);
}

// Copy the device tree into an internal format. Then use the functions in this file to query
// the tree after this during initAllSystems and initDevices.
Error initDeviceTree(uint8_t* dtb) {
    if (read32be(dtb) != 0xd00dfeed) {
        return someError(EINVAL, "Invalid device tree magic");
    }
    size_t tree_size = read32be(dtb + 4);
    DEBUG_DEVTREE("[dtb] size: %u\n", tree_size);
    DEBUG_DEVTREE("[dtb] version: %u\n", read32be(dtb + 20));
    uint32_t comp_version = read32be(dtb + 24);
    if (comp_version > 17) {
        KERNEL_WARNING("Unsupported device tree format version");
    }
    DEBUG_DEVTREE("[dtb] boot cpu: %u\n", read32be(dtb + 28));
    device_tree.raw = kalloc(tree_size);
    memcpy(device_tree.raw, dtb, tree_size);
    CHECKED(parseReservedMemoryRegions(device_tree.raw + read32be(dtb + 16)));
    CHECKED(parseDeviceTreeNodes(
        device_tree.raw + read32be(dtb + 8), (char*)device_tree.raw + read32be(dtb + 12)
    ));
    KERNEL_SUBSUCCESS("Initialized device tree information");
    return simpleError(SUCCESS);
}

