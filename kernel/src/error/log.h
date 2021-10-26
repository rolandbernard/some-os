#ifndef _LOG_H_
#define _LOG_H_

#include "error/error.h"
#include "util/text.h"
#include "util/macro.h"
#include "interrupt/syscall.h"

// Initialize the log system
Error initLogSystem();

// Log the given message
Error logKernelMessage(const char* fmt, ...);

#ifdef DEBUG
#define KERNEL_LOG(FMT, ...) { \
    logKernelMessage(FMT "\n \e[2;3m∟ <%s>\t" __FILE__ ":" STRINGX(__LINE__) "\e[m\n" __VA_OPT__(,) __VA_ARGS__, __PRETTY_FUNCTION__); \
}
#else
#define KERNEL_LOG(FMT, ...) { \
    logKernelMessage(FMT "\n" __VA_OPT__(,) __VA_ARGS__); \
}
#endif

void printSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

#endif
