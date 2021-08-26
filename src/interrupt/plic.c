
#include "interrupt/plic.h"

#include "memory/memmap.h"

void handleExternalInterrupt() {
}

void setInterruptFunction(ExternalInterrupt id, ExternalInterruptFunction function, void* udata) {
}

void clearInterruptFunction(ExternalInterrupt timeout) {
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

