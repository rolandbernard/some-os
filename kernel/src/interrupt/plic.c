
#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "interrupt/plic.h"

#include "error/log.h"
#include "memory/memmap.h"
#include "memory/kalloc.h"
#include "task/syscall.h"

typedef struct InterruptEntry_s {
    struct InterruptEntry_s* next;
    ExternalInterrupt id;
    ExternalInterruptFunction function;
    void* udata;
} InterruptEntry;

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
    TrapFrame* lock = criticalEnter();
    lockSpinLock(&plic_lock);
    entry->next = interrupts;
    interrupts = entry; 
    unlockSpinLock(&plic_lock);
    criticalReturn(lock);
    enableInterrupt(id);
}

void clearInterruptFunction(ExternalInterrupt id, ExternalInterruptFunction function, void* udata) {
    bool interrupt_used = false;
    TrapFrame* lock = criticalEnter();
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
    criticalReturn(lock);
    unlockSpinLock(&plic_lock);
    if (!interrupt_used) {
        disableInterrupt(id);
    }
}

void enableInterrupt(ExternalInterrupt id) {
    uint32_t offset = id / 32;
    uint32_t bit_value = 1 << (id % 32);
    volatile uint32_t* address = (volatile uint32_t*)(memory_map[VIRT_PLIC].base + 0x2000 + offset);
    *address = *address | bit_value;
}

void disableInterrupt(ExternalInterrupt id) {
    uint32_t offset = id / 32;
    uint32_t bit_value = 1 << (id % 32);
    volatile uint32_t* address = (volatile uint32_t*)(memory_map[VIRT_PLIC].base + 0x2000 + offset);
    *address = *address & ~bit_value;
}

void setInterruptPriority(ExternalInterrupt id, InterruptPriority priority) {
    priority &= 0x111; // Maximum priority is 7
    *((volatile uint32_t*)memory_map[VIRT_PLIC].base + id) = priority;;
}

void setPlicPriorityThreshold(InterruptPriority priority) {
    priority &= 0x111; // Maximum priority is 7
    *(volatile uint32_t*)(memory_map[VIRT_PLIC].base + 0x200000) = priority;;
}

ExternalInterrupt nextInterrupt() {
    return *(volatile ExternalInterrupt*)(memory_map[VIRT_PLIC].base + 0x200004);
}

void completeInterrupt(ExternalInterrupt id) {
    *(volatile ExternalInterrupt*)(memory_map[VIRT_PLIC].base + 0x200004) = id;
}

Error initPlic() {
    setPlicPriorityThreshold(0);
    KERNEL_LOG("[>] Initialized PLIC");
    return simpleError(SUCCESS);
}

