
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "devices/serial/tty.h"

#include "memory/kalloc.h"
#include "task/schedule.h"
#include "task/syscall.h"
#include "util/text.h"
#include "util/util.h"

#define INITIAL_TTY_BUFFER_CAPACITY 512

static void waitForReadOperation(void* _, Task* task, UartTtyDevice* dev) {
    moveTaskToState(task, WAITING);
    enqueueTask(task);
    task->sched.sched_next = dev->blocked;
    dev->blocked = task;
    unlockSpinLock(&dev->lock);
    runNextTask();
}

static Error uartTtyReadOrWait(UartTtyDevice* dev, VirtPtr buffer, size_t size, size_t* read, bool block) {
    Task* task = criticalEnter();
    lockSpinLock(&dev->lock);
    if (dev->buffer_count == 0) {
        if (block) {
            assert(task != NULL);
            if (saveToFrame(&task->frame)) {
                callInHart((void*)waitForReadOperation, task, dev);
            }
        } else {
            unlockSpinLock(&dev->lock);
        }
        return simpleError(EAGAIN);
    } else {
        size_t length = umin(dev->buffer_count, size);
        if (dev->buffer_capacity < dev->buffer_start + length) {
            // We need to wrap around
            size_t first = dev->buffer_capacity - dev->buffer_start;
            memcpyBetweenVirtPtr(buffer, virtPtrForKernel(dev->buffer + dev->buffer_start), first);
            size_t second = length - first;
            buffer.address += first;
            memcpyBetweenVirtPtr(buffer, virtPtrForKernel(dev->buffer), second);
        } else {
            memcpyBetweenVirtPtr(buffer, virtPtrForKernel(dev->buffer + dev->buffer_start), length);
        }
        dev->buffer_start = (dev->buffer_start + length) % dev->buffer_capacity;
        dev->buffer_count -= length;
        unlockSpinLock(&dev->lock);
        *read = length;
        return simpleError(SUCCESS);
    }
}

static Error uartTtyReadFunction(UartTtyDevice* dev, VirtPtr buffer, size_t size, size_t* read, bool block) {
    Error err;
    do {
        err = uartTtyReadOrWait(dev, buffer, size, read, block);
    } while (block && err.kind == EAGAIN);
    return err;
}

static Error uartTtyWriteFunction(UartTtyDevice* dev, VirtPtr buffer, size_t size, size_t* written) {
    size_t count = 0;
    size_t part_count = getVirtPtrParts(buffer, size, NULL, 0, false);
    VirtPtrBufferPart parts[part_count];
    getVirtPtrParts(buffer, size, parts, part_count, false);
    lockSpinLock(&dev->lock);
    for (size_t i = 0; i < part_count; i++) {
        for (size_t j = 0; j < parts[i].length; j++) {
            Error err;
            do {
                err = dev->write_func(dev->uart_data, ((char*)parts[i].address)[j]);
            } while (err.kind == EBUSY);
            if (isError(err)) {
                unlockSpinLock(&dev->lock);
                *written = count;
                return err;
            }
            count++;
        }
    }
    unlockSpinLock(&dev->lock);
    *written = count;
    return simpleError(SUCCESS);
}

static size_t uartTtyAvailFunction(UartTtyDevice* dev) {
    lockSpinLock(&dev->lock);
    size_t ret = dev->buffer_count;
    unlockSpinLock(&dev->lock);
    return ret;
}

static void uartTtyFlushFunction(UartTtyDevice* dev) {
    lockSpinLock(&dev->lock);
    dev->buffer_count = 0;
    unlockSpinLock(&dev->lock);
}

static void uartTtyResizeBuffer(UartTtyDevice* dev) {
    size_t capacity = dev->buffer_capacity * 3 / 2;
    dev->buffer = krealloc(dev->buffer, capacity);
    if (dev->buffer_capacity < dev->buffer_start + dev->buffer_count) {
        size_t to_move = dev->buffer_start - dev->buffer_capacity + dev->buffer_count;
        assert(dev->buffer_capacity + to_move <= capacity);
        memmove(dev->buffer + dev->buffer_capacity, dev->buffer, to_move);
    }
    dev->buffer_capacity = capacity;
}

void uartTtyDataReady(UartTtyDevice* dev) {
    Error err;
    lockSpinLock(&dev->lock);
    do {
        if (dev->buffer_count == dev->buffer_capacity) {
            uartTtyResizeBuffer(dev);
        }
        char* new = dev->buffer + (dev->buffer_start + dev->buffer_count) % dev->buffer_capacity;
        err = dev->read_func(dev->uart_data, new);
        if (!isError(err)) {
            dev->buffer_count++;
        }
    } while (!isError(err));
    while (dev->blocked != NULL) {
        Task* wakeup = dev->blocked;
        dev->blocked = wakeup->sched.sched_next;
        moveTaskToState(wakeup, ENQUABLE);
        enqueueTask(wakeup);
    }
    unlockSpinLock(&dev->lock);
}

static const CharDeviceFunctions funcs = {
    .read = (CharDeviceReadFunction)uartTtyReadFunction,
    .write = (CharDeviceWriteFunction)uartTtyWriteFunction,
    .avail = (CharDeviceAvailFunction)uartTtyAvailFunction,
    .flush = (CharDeviceFlushFunction)uartTtyFlushFunction,
};

UartTtyDevice* createUartTtyDevice(void* uart, UartWriteFunction write, UartReadFunction read) {
    UartTtyDevice* dev = kalloc(sizeof(UartTtyDevice));
    dev->base.base.type = DEVICE_CHAR;
    dev->base.base.name = "tty";
    dev->base.functions = &funcs;
    dev->uart_data = uart;
    dev->write_func = write;
    dev->read_func = read;
    dev->buffer_start = 0;
    dev->buffer_count = 0;
    dev->buffer_capacity = 0;
    dev->buffer = NULL;
    dev->blocked = NULL;
    initSpinLock(&dev->lock);
    return dev;
}

Error writeStringToTty(CharDevice* dev, const char* str) {
    return writeStringNToTty(dev, str, strlen(str));
}

Error writeStringNToTty(CharDevice* dev, const char* str, size_t length) {
    size_t ignored;
    return dev->functions->write(dev, virtPtrForKernelConst(str), length, &ignored);
}

Error writeToTty(CharDevice* dev, const char* fmt, ...) {
    FORMAT_STRING(string, fmt);
    return writeStringToTty(dev, string);
}

