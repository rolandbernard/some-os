
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "interrupt/plic.h"

#include "memory/memmap.h"
#include "memory/kalloc.h"

typedef struct {
    ExternalInterrupt id;
    ExternalInterruptFunction function;
    void* udata;
} InterruptEntry;

static InterruptEntry* interrupts = NULL;
static size_t capacity = 0;
static size_t length = 0;

void handleExternalInterrupt() {
    ExternalInterrupt interrupt = nextInterrupt();
    if (interrupt != 0) {
        for (size_t i = 0; i < length; i++) {
            if (interrupts[i].id == interrupt) {
                interrupts[i].function(interrupt, interrupts[i].udata);
            }
        }
        completeInterrupt(interrupt);
    }
}

void setInterruptFunction(ExternalInterrupt id, ExternalInterruptFunction function, void* udata) {
    if (capacity <= length) {
        capacity += 32;
        interrupts = krealloc(interrupts, capacity * sizeof(InterruptEntry));
    }
    interrupts[length].id = id;
    interrupts[length].function = function;
    interrupts[length].udata = udata;
    length++;
    enableInterrupt(id);
}

void clearInterruptFunction(ExternalInterrupt id, ExternalInterruptFunction function, void* udata) {
    bool interrupt_used = false;
    for (size_t i = 0; i < length;) {
        if (interrupts[i].id == id && interrupts[i].function == function && interrupts[i].udata == udata) {
            memmove(interrupts + i, interrupts + i + 1, length - i - 1);
            length--;
        } else {
            if (interrupts[i].id == id) {
                interrupt_used = true;
            }
            i++;
        }
    }
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
    *(volatile uint32_t*)(memory_map[VIRT_PLIC].base + id) = priority;;
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

