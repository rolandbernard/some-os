
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
#define MAX_TTY_BUFFER_CAPACITY 4096

static char getBuffer(UartTtyDevice* dev, size_t index) {
    return dev->buffer[(dev->buffer_start + index) % dev->buffer_capacity];
}

static size_t bufferLineLength(UartTtyDevice* dev) {
    for (size_t i = 0; i < dev->buffer_count; i++) {
        char c = getBuffer(dev, i);
        if (c == '\n' || c == dev->ctrl.cc[VEOL]) {
            return i + 1;
        } else if (c == dev->ctrl.cc[VEOF]) {
            return i;
        }
    }
    return 0;
}

static bool canReturnRead(UartTtyDevice* dev, Task* task) {
    return dev->buffer_count > 0 && (
        (dev->ctrl.lflag & ICANON) == 0
        || getBuffer(dev, 0) == dev->ctrl.cc[VEOF]
        || dev->line_delim_count > 0);
}

static void wakeupIfRequired(UartTtyDevice* dev) {
    Task** current = &dev->blocked;
    while (*current != NULL) {
        if (canReturnRead(dev, *current)) {
            Task* wakeup = *current;
            *current = wakeup->sched.sched_next;
            moveTaskToState(wakeup, ENQUABLE);
            enqueueTask(wakeup);
        } else {
            current = &(*current)->sched.sched_next;
        }
    }
}

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
    if (canReturnRead(dev, task)) {
        size_t size_available = (dev->ctrl.lflag & ICANON) != 0 ? bufferLineLength(dev) : dev->buffer_count;
        size_t length = umin(size_available, size);
        for (size_t i = 0; i < length; i++) {
            char c = getBuffer(dev, i);
            if (c == '\n' || c == dev->ctrl.cc[VEOL] || c == dev->ctrl.cc[VEOF]) {
                dev->line_delim_count--;
            }
        }
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
        if ((dev->ctrl.lflag & ICANON) != 0 && dev->buffer_count > 0 && getBuffer(dev, 0) == dev->ctrl.cc[VEOF]) {
            dev->line_delim_count--;
            dev->buffer_count--;
        }
        unlockSpinLock(&dev->lock);
        criticalReturn(task);
        *read = length;
        return simpleError(SUCCESS);
    } else {
        if (block) {
            assert(task != NULL);
            if (saveToFrame(&task->frame)) {
                callInHart((void*)waitForReadOperation, task, dev);
            }
        } else {
            unlockSpinLock(&dev->lock);
            criticalReturn(task);
        }
        return simpleError(EAGAIN);
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
    size_t capacity = dev->buffer_capacity == 0
        ? INITIAL_TTY_BUFFER_CAPACITY : dev->buffer_capacity * 3 / 2;
    if (capacity > MAX_TTY_BUFFER_CAPACITY) {
        capacity = MAX_TTY_BUFFER_CAPACITY;
    }
    if (capacity != dev->buffer_capacity) {
        dev->buffer = krealloc(dev->buffer, capacity);
        if (dev->buffer_capacity < dev->buffer_start + dev->buffer_count) {
            size_t to_move = dev->buffer_start - dev->buffer_capacity + dev->buffer_count;
            assert(dev->buffer_capacity + to_move <= capacity);
            memmove(dev->buffer + dev->buffer_capacity, dev->buffer, to_move);
        }
        dev->buffer_capacity = capacity;
    }
}

static Error basicTtyWrite(UartTtyDevice* dev, char character) {
    Error error = simpleError(SUCCESS);
    if (character == '\r' && (dev->ctrl.oflag & ONOCR) != 0) {
        return simpleError(SUCCESS);
    }
    if (character == '\r' && (dev->ctrl.oflag & OCRNL) != 0) {
        character = '\n';
    }
    if (character == '\n' && (dev->ctrl.oflag & ONLCR) != 0) {
        do {
            error = dev->write_func(dev->uart_data, '\r');
        } while (error.kind == EBUSY);
    }
    if (!isError(error)) {
        do {
            error = dev->write_func(dev->uart_data, character);
        } while (error.kind == EBUSY);
    }
    return error;
}

static bool isTerminalSpecialChar(char c) {
    return c < 0x20 && c >= 2 && c != '\t' && c != '\n';
}

static Error basicTtyRead(UartTtyDevice* dev) {
    // TODO: Implement at least the following flags
    // * ISIG
    // * VTIME
    // * VMIN
    // * VINTR
    // * VKILL
    // * VQUIT
    if (dev->buffer_count == dev->buffer_capacity) {
        uartTtyResizeBuffer(dev);
    }
    if (dev->buffer_count == dev->buffer_capacity) {
        dev->buffer_count--; // Input is truncated after this
    }
    char* new = dev->buffer + (dev->buffer_start + dev->buffer_count) % dev->buffer_capacity;
    Error error = dev->read_func(dev->uart_data, new);
    if (!isError(error)) {
        if (*new == '\r') {
            if ((dev->ctrl.iflag & IGNCR) != 0) {
                return error;
            } else if ((dev->ctrl.iflag & ICRNL) != 0) {
                *new = '\n';
            }
        } else if (*new == '\n') {
            if ((dev->ctrl.iflag & INLCR) != 0) {
                *new = '\r';
            }
        }
        if (*new == dev->ctrl.cc[VERASE] && (dev->ctrl.lflag & ECHOE) != 0 && (dev->ctrl.lflag & ICANON) != 0) {
            if (dev->buffer_count > 0) {
                dev->buffer_count--;
                size_t count = (dev->ctrl.lflag & ECHOCTL) != 0
                    && isTerminalSpecialChar(getBuffer(dev, dev->buffer_count)) ? 2 : 1;
                for (size_t i = 0; i < count; i++) {
                    basicTtyWrite(dev, '\b');
                    basicTtyWrite(dev, ' ');
                    basicTtyWrite(dev, '\b');
                }
            }
            return error;
        }
        if (*new == '\n' || *new == dev->ctrl.cc[VEOL] || *new == dev->ctrl.cc[VEOF]) {
            dev->line_delim_count++;
        }
        dev->buffer_count++;
        if (
            ((dev->ctrl.lflag & ECHO) != 0
                || ((dev->ctrl.lflag & ECHONL) != 0 && (dev->ctrl.lflag & ICANON) != 0 && *new == '\n'))
            && ((dev->ctrl.lflag & ICANON) == 0 || *new != dev->ctrl.cc[VEOF])
        ) {
            if ((dev->ctrl.lflag & ECHOCTL) != 0 && isTerminalSpecialChar(*new)) {
                basicTtyWrite(dev, '^');
                basicTtyWrite(dev, *new + 0x40);
            } else {
                basicTtyWrite(dev, *new);
            }
        }
    }
    return error;
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
            memcpyBetweenVirtPtr(argp, virtPtrForKernel(&dev->ctrl), sizeof(Termios));
            return simpleError(SUCCESS);
        case TCSETSF:
            dev->buffer_count = 0;
            // fall through
        case TCSETSW:
        case TCSETS:
            memcpyBetweenVirtPtr(virtPtrForKernel(&dev->ctrl), argp, sizeof(Termios));
            wakeupIfRequired(dev); // Wakeup might be cause by change of flags.
            return simpleError(SUCCESS);
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
    wakeupIfRequired(dev);
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
    memset(&dev->ctrl, 0, sizeof(Termios));
    dev->ctrl.iflag = ICRNL;
    dev->ctrl.oflag = ONLCR | ONOCR | OPOST;
    dev->ctrl.lflag = ECHO | ECHOE | ECHONL | ECHOCTL | ICANON | ISIG;
    dev->ctrl.cc[VERASE] = '\x7f';
    dev->ctrl.cc[VEOF] = '\x04';
    dev->ctrl.cc[VEOL] = '\x00';
    dev->ctrl.cc[VINTR] = '\x03';
    dev->ctrl.cc[VKILL] = '\x15';
    dev->ctrl.cc[VQUIT] = '\x1c';
    dev->ctrl.cc[VTIME] = 0;
    dev->ctrl.cc[VMIN] = 1;
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

