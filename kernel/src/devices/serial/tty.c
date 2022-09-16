
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

static bool canReturnRead(UartTtyDevice* dev) {
    if ((dev->ctrl.lflag & ICANON) == 0) {
        if (dev->ctrl.cc[VTIME] == 0) {
            return dev->buffer_count >= dev->ctrl.cc[VMIN];
        } else {
            return dev->buffer_count >= umax(dev->ctrl.cc[VMIN], 1)
               || (dev->buffer_count >= umin(dev->ctrl.cc[VMIN], 1)
                   && dev->last_byte + dev->ctrl.cc[VTIME] * CLOCKS_PER_SEC / 10 < getTime());
        }
    } else {
        return dev->line_delim_count > 0;
    }
}

static bool wakeupIfRequired(UartTtyDevice* dev) {
    if (canReturnRead(dev)) {
        while (dev->blocked != NULL) {
            Task* wakeup = dev->blocked;
            dev->blocked = wakeup->sched.sched_next;
            moveTaskToState(wakeup, ENQUABLE);
            enqueueTask(wakeup);
        }
        return true;
    } else {
        return false;
    }
}

static void checkForWakeup(Time time, void* udata) {
    UartTtyDevice* dev = (UartTtyDevice*)udata;
    lockSpinLock(&dev->lock);
    if (!wakeupIfRequired(dev)) {
        setTimeout(dev->ctrl.cc[VTIME] * CLOCKS_PER_SEC / 10, checkForWakeup, dev);
    }
    unlockSpinLock(&dev->lock);
}

static void waitForReadOperation(void* _, Task* task, UartTtyDevice* dev) {
    moveTaskToState(task, WAITING);
    enqueueTask(task);
    task->sched.sched_next = dev->blocked;
    dev->blocked = task;
    if ((dev->ctrl.lflag & ICANON) == 0 && dev->ctrl.cc[VTIME] != 0 && dev->blocked == NULL) {
        setTimeout(dev->ctrl.cc[VTIME] * CLOCKS_PER_SEC / 10, checkForWakeup, dev);
    }
    unlockSpinLock(&dev->lock);
    runNextTask();
}

static Error uartTtyReadOrWait(UartTtyDevice* dev, VirtPtr buffer, size_t size, size_t* read, bool block) {
    Task* task = criticalEnter();
    lockSpinLock(&dev->lock);
    if ((dev->ctrl.lflag & ICANON) == 0 && dev->ctrl.cc[VTIME] != 0 && dev->blocked == NULL) {
        dev->last_byte = getTime();
    }
    if (canReturnRead(dev)) {
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

static void eraseLastCharacter(UartTtyDevice* dev) {
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
}

static Error basicTtyRead(UartTtyDevice* dev) {
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
        if ((dev->ctrl.lflag & ECHOE) != 0 && (dev->ctrl.lflag & ICANON) != 0 && *new == dev->ctrl.cc[VERASE]) {
            eraseLastCharacter(dev);
            return error;
        }
        if ((dev->ctrl.lflag & ECHOK) != 0 && (dev->ctrl.lflag & ICANON) != 0 && *new == dev->ctrl.cc[VKILL]) {
            while (dev->buffer_count > 0) {
                eraseLastCharacter(dev);
            }
            return error;
        }
        if ((dev->ctrl.lflag & ISIG) != 0 && (*new == dev->ctrl.cc[VINTR] || *new == dev->ctrl.cc[VQUIT] || *new == dev->ctrl.cc[VSUSP])) {
            if (*new == dev->ctrl.cc[VINTR]) {
                signalProcessGroup(dev->process_group, SIGINT);
            } else if (*new == dev->ctrl.cc[VQUIT]) {
                signalProcessGroup(dev->process_group, SIGQUIT);
            } else if (*new == dev->ctrl.cc[VSUSP]) {
                signalProcessGroup(dev->process_group, SIGTSTP);
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
            wakeupIfRequired(dev); // Wakeup might be caused by change of flags.
            return simpleError(SUCCESS);
        case TIOCGPGRP:
            writeInt(argp, sizeof(Pid) * 8, dev->process_group);
            return simpleError(SUCCESS);
        case TIOCSPGRP:
            dev->process_group = readInt(argp, sizeof(Pid) * 8);
            return simpleError(SUCCESS);
        case TCXONC:
            return simpleError(ENOTSUP);
        case TCFLSH:
            dev->buffer_count = 0;
            return simpleError(SUCCESS);
        case FIONREAD:
            writeInt(argp, sizeof(int) * 8, dev->buffer_count);
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

static bool uartTtyWillBlockFunction(UartTtyDevice* dev, bool write) {
    return !write && !canReturnRead(dev);
}

static const CharDeviceFunctions funcs = {
    .read = (CharDeviceReadFunction)uartTtyReadFunction,
    .write = (CharDeviceWriteFunction)uartTtyWriteFunction,
    .ioctl = (CharDeviceIoctlFunction)uartTtyIoctlFunction,
    .will_block = (CharDeviceWillBlockFunction)uartTtyWillBlockFunction,
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
    dev->ctrl.lflag = ECHO | ECHOK | ECHOE | ECHONL | ECHOCTL | ICANON | ISIG;
    dev->ctrl.cc[VERASE] = '\x7f';
    dev->ctrl.cc[VEOF] = '\x04';
    dev->ctrl.cc[VEOL] = '\x00';
    dev->ctrl.cc[VINTR] = '\x05';
    dev->ctrl.cc[VKILL] = '\x15';
    dev->ctrl.cc[VQUIT] = '\x06';
    dev->ctrl.cc[VSUSP] = '\x14';
    dev->ctrl.cc[VTIME] = 0;
    dev->ctrl.cc[VMIN] = 1;
    dev->last_byte = 0;
    dev->line_delim_count = 0;
    dev->process_group = 1; // Just give it to the init process
    return dev;
}

void uartTtyDataReady(UartTtyDevice* dev) {
    Error error;
    lockSpinLock(&dev->lock);
    do {
        error = basicTtyRead(dev);
    } while (!isError(error));
    dev->last_byte = getTime();
    wakeupIfRequired(dev);
    unlockSpinLock(&dev->lock);
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

