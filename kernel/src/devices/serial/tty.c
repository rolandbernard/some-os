
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

static Error basicTtyRead(UartTtyDevice* dev) {
    // TODO: Implement at least the following flags
    // * INLCR
    // * IGNCR
    // * ICRNL
    // * ISIG
    // * ICANON
    // * ECHO
    // * ECHOE
    // * ECHOK
    // * ECHONL
    // * VTIME
    // * VMIN
    // * VEOF
    // * VEOL
    // * VERASE
    // * VINTR
    // * VKILL
    // * VQUIT
    if (dev->buffer_count == dev->buffer_capacity) {
        uartTtyResizeBuffer(dev);
    }
    char* new = dev->buffer + (dev->buffer_start + dev->buffer_count) % dev->buffer_capacity;
    Error error = dev->read_func(dev->uart_data, new);
    if (!isError(error)) {
        dev->buffer_count++;
    }
    return error;
}

static Error basicTtyWrite(UartTtyDevice* dev, char character) {
    // TODO: Implement at least the following flags
    // * ONLCR
    // * OCRNL
    // * ONOCR
    // * ONLRET
    Error err;
    do {
        err = dev->write_func(dev->uart_data, character);
    } while (err.kind == EBUSY);
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
            Error error = basicTtyWrite(dev, ((char*)parts[i].address)[j]);
            if (isError(error)) {
                unlockSpinLock(&dev->lock);
                *written = count;
                return error;
            }
            count++;
        }
    }
    unlockSpinLock(&dev->lock);
    *written = count;
    return simpleError(SUCCESS);
}

static Error basicUartTtyIoctlFunction(UartTtyDevice* dev, size_t request, VirtPtr argp, uintptr_t* res) {
    switch (request) {
        case TCGETS:
            return simpleError(ENOTSUP);
        case TCSETSF:
            dev->buffer_count = 0;
            // fall through
        case TCSETS:
        case TCSETSW:
            return simpleError(ENOTSUP);
        case TIOCGPGRP:
            return simpleError(ENOTSUP);
        case TIOCSPGRP:
            return simpleError(ENOTSUP);
        case TCXONC:
            return simpleError(ENOTSUP);
        case TCFLSH:
            dev->buffer_count = 0;
            return simpleError(SUCCESS);
        case FIONREAD:
            *res = dev->buffer_count;
            return simpleError(SUCCESS);
        default:
            return simpleError(ENOTTY);
    }
}

static Error uartTtyIoctlFunction(UartTtyDevice* dev, size_t request, VirtPtr argp, uintptr_t* res) {
    lockSpinLock(&dev->lock);
    Error error = basicUartTtyIoctlFunction(dev, request, argp, res);
    unlockSpinLock(&dev->lock);
    return error;
}

void uartTtyDataReady(UartTtyDevice* dev) {
    Error error;
    lockSpinLock(&dev->lock);
    do {
        error = basicTtyRead(dev);
    } while (!isError(error));
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
    .ioctl = (CharDeviceIoctlFunction)uartTtyIoctlFunction,
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
    // TODO: Init to correct defaults
    memset(&dev->ctrl, 0, sizeof(Termios));
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

