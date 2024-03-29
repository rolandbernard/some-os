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
    struct Driver_s* driver;
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

typedef Error (*DeviceTreeNodeCallback)(DeviceTreeNode* node, void* udata);

Error forAllDeviceTreeNodesDo(DeviceTreeNodeCallback callback, void* udata);

DeviceTreeNode* findNodeAtPath(const char* path);

DeviceTreeProperty* findNodeProperty(DeviceTreeNode* node, const char* prop);

uint32_t readPropertyU32(DeviceTreeProperty* prop, size_t n);

uint64_t readPropertyU64(DeviceTreeProperty* prop, size_t n);

const char* readPropertyString(DeviceTreeProperty* prop, size_t n);

uint32_t readPropertyU32OrDefault(DeviceTreeProperty* prop, size_t n, uint32_t def);

uint64_t readPropertyU64OrDefault(DeviceTreeProperty* prop, size_t n, uint64_t def);

const char* readPropertyStringOrDefault(DeviceTreeProperty* prop, size_t n, const char* def);

#endif
