
#include <string.h>

#include "devices/serial/uart16550.h"

#include "devices/driver.h"
#include "devices/serial/tty.h"
#include "error/log.h"
#include "memory/kalloc.h"
#include "task/spinlock.h"

#define REG(OFF) uart->base_address[(OFF) << uart->reg_shift]

Error initUart16550(Uart16550* uart) {
    lockSpinLock(&uart->lock);
    // Set the word length to 8-bits by writing 1 into LCR[1:0]
    REG(3) = (1 << 0) | (1 << 1);
    // Enable FIFO
    REG(2) = 1 << 0;
    uint16_t divisor = 600;
    // Enable divisor latch
    uint8_t lcr = REG(3);
    REG(3) = lcr | 1 << 7;
    // Write divisor
    REG(0) = divisor & 0xff;
    REG(1) = divisor >> 8;
    // Close divisor latch
    REG(3) = lcr;
    // Enable the data ready interrupt
    REG(1) = 1 << 0;
    REG(4) = 1 << 3;
    unlockSpinLock(&uart->lock);
    registerUart16550(uart);
    KERNEL_SUBSUCCESS("Initialized UART device");
    return simpleError(SUCCESS);
}

static Error writeUart16550(Uart16550* uart, char value) {
    lockSpinLock(&uart->lock);
    if (((REG(5) >> 5) & 0x1) == 0) {
        /* THR is not empty */
        unlockSpinLock(&uart->lock);
        return simpleError(EBUSY);
    } else {
        // Write directly to MMIO
        REG(0) = value;
        unlockSpinLock(&uart->lock);
        return simpleError(SUCCESS);
    }
}

static Error readUart16550(Uart16550* uart, char* value) {
    lockSpinLock(&uart->lock);
    if ((REG(5) & 0x1) == 0) {
        // Data Ready == 0 => No data is available
        unlockSpinLock(&uart->lock);
        return simpleError(EAGAIN);
    } else {
        // Data is available
        *value = REG(0);
        unlockSpinLock(&uart->lock);
        return simpleError(SUCCESS);
    }
}

static void handleInterrupt(ExternalInterrupt id, void* udata) {
    uartTtyDataReady((UartTtyDevice*)udata);
}

Error registerUart16550(Uart16550* uart) {
    UartTtyDevice* dev = createUartTtyDevice(
        uart, (UartWriteFunction)writeUart16550, (UartReadFunction)readUart16550
    );
    ExternalInterrupt itr_id = uart->interrupt;
    setInterruptFunction(itr_id, handleInterrupt, dev);
    setInterruptPriority(itr_id, 1);
    registerDevice((Device*)dev);
    return simpleError(SUCCESS);
}

static bool checkDeviceCompatibility(const char* name) {
    return strstr(name, "ns16550") != NULL;
}

static Error initDeviceFor(DeviceTreeNode* node) {
    DeviceTreeProperty* reg = findNodeProperty(node, "reg");
    if (reg == NULL) {
        return simpleError(ENXIO);
    }
    Uart16550* dev = kalloc(sizeof(Uart16550));
    initSpinLock(&dev->lock);
    dev->base_address = (uint8_t*)readPropertyU64(reg, 0);
    dev->interrupt = readPropertyU32OrDefault(findNodeProperty(node, "interrupts"), 0, 0);
    dev->ref_clock = readPropertyU32OrDefault(findNodeProperty(node, "clock-frequency"), 0, 0);
    dev->reg_shift = readPropertyU32OrDefault(findNodeProperty(node, "reg-shift"), 0, 0);
    initUart16550(dev);
    return simpleError(SUCCESS);
}

Error registerDriverUart16550() {
    Driver* driver = kalloc(sizeof(Driver));
    driver->name = "uart16550";
    driver->flags = DRIVER_FLAGS_MMIO | DRIVER_FLAGS_STDOUT;
    driver->check = checkDeviceCompatibility;
    driver->init = initDeviceFor;
    registerDriver(driver);
    return simpleError(SUCCESS);
}

