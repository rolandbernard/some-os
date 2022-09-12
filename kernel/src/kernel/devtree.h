#ifndef _KERNEL_DEVTREE_H_
#define _KERNEL_DEVTREE_H_

#include <stdint.h>

#include "error/error.h"
#include "devices/devices.h"

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
    Device* device;
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

Error initDeviceTree(uint8_t* dtb);

DeviceTreeNode* findNodeAtPath(const char* path);

DeviceTreeProperty* findNodeProperty(DeviceTreeNode* node, const char* prop);

#endif
