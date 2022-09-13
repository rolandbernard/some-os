
#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "interrupt/plic.h"

#include "devices/driver.h"
#include "error/log.h"
#include "memory/kalloc.h"
#include "task/syscall.h"

typedef struct InterruptEntry_s {
    struct InterruptEntry_s* next;
    ExternalInterrupt id;
    ExternalInterruptFunction function;
    void* udata;
} InterruptEntry;

static uintptr_t plic_base_addr;
static SpinLock plic_lock;
static InterruptEntry* interrupts = NULL;

void handleExternalInterrupt() {
    ExternalInterrupt interrupt = nextInterrupt();
    while (interrupt != 0) {
        lockSpinLock(&plic_lock);
        size_t interrupt_count = 0;
        InterruptEntry* current = interrupts;
        while (current != NULL) {
            if (current->id == interrupt) {
                interrupt_count++;
            }
            current = current->next;
        }
        InterruptEntry* interrupts_to_run[interrupt_count];
        interrupt_count = 0;
        current = interrupts;
        while (current != NULL) {
            if (current->id == interrupt) {
                interrupts_to_run[interrupt_count] = current;
                interrupt_count++;
            }
            current = current->next;
        }
        unlockSpinLock(&plic_lock);
        for (size_t i = 0; i < interrupt_count; i++) {
            interrupts_to_run[i]->function(interrupt, interrupts_to_run[i]->udata);
        }
        completeInterrupt(interrupt);
        interrupt = nextInterrupt();
    }
}

void setInterruptFunction(ExternalInterrupt id, ExternalInterruptFunction function, void* udata) {
    InterruptEntry* entry = kalloc(sizeof(InterruptEntry));
    assert(entry != NULL);
    entry->id = id;
    entry->function = function;
    entry->udata = udata;
    lockSpinLock(&plic_lock);
    entry->next = interrupts;
    interrupts = entry; 
    unlockSpinLock(&plic_lock);
    enableInterrupt(id);
}

void clearInterruptFunction(ExternalInterrupt id, ExternalInterruptFunction function, void* udata) {
    bool interrupt_used = false;
    lockSpinLock(&plic_lock);
    InterruptEntry** current = &interrupts;
    while (*current != NULL) {
        if ((*current)->id == id && (*current)->function == function && (*current)->udata == udata) {
            InterruptEntry* to_remove = *current;
            *current = to_remove->next;
            dealloc(to_remove);
        } else {
            if ((*current)->id == id) {
                interrupt_used = true;
            }
            current = &(*current)->next;
        }
    }
    unlockSpinLock(&plic_lock);
    if (!interrupt_used) {
        disableInterrupt(id);
    }
}

void enableInterrupt(ExternalInterrupt id) {
    uint32_t offset = id / 32;
    uint32_t bit_value = 1 << (id % 32);
    volatile uint32_t* address = (volatile uint32_t*)(plic_base_addr + 0x2000 + offset);
    *address = *address | bit_value;
}

void disableInterrupt(ExternalInterrupt id) {
    uint32_t offset = id / 32;
    uint32_t bit_value = 1 << (id % 32);
    volatile uint32_t* address = (volatile uint32_t*)(plic_base_addr + 0x2000 + offset);
    *address = *address & ~bit_value;
}

void setInterruptPriority(ExternalInterrupt id, InterruptPriority priority) {
    priority &= 0x111; // Maximum priority is 7
    *((volatile uint32_t*)plic_base_addr + id) = priority;;
}

void setPlicPriorityThreshold(InterruptPriority priority) {
    priority &= 0x111; // Maximum priority is 7
    *(volatile uint32_t*)(plic_base_addr + 0x200000) = priority;;
}

ExternalInterrupt nextInterrupt() {
    return *(volatile ExternalInterrupt*)(plic_base_addr + 0x200004);
}

void completeInterrupt(ExternalInterrupt id) {
    *(volatile ExternalInterrupt*)(plic_base_addr + 0x200004) = id;
}

static Error initPlic() {
    setPlicPriorityThreshold(0);
    KERNEL_SUBSUCCESS("Initialized PLIC");
    return simpleError(SUCCESS);
}

static bool checkDeviceCompatibility(const char* name) {
    return strstr(name, "plic") != NULL;
}

static Error initDeviceFor(DeviceTreeNode* node) {
    DeviceTreeProperty* reg = findNodeProperty(node, "reg");
    if (reg == NULL) {
        return simpleError(ENXIO);
    }
    plic_base_addr = readPropertyU64(reg, 0);
    initPlic();
    return simpleError(SUCCESS);
}

Error registerDriverPlic() {
    Driver* driver = kalloc(sizeof(Driver));
    driver->name = "riscv-plic";
    driver->flags = DRIVER_FLAGS_MMIO | DRIVER_FLAGS_INTERRUPT;
    driver->check = checkDeviceCompatibility;
    driver->init = initDeviceFor;
    registerDriver(driver);
    return simpleError(SUCCESS);
}

