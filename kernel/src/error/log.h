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

#define STYLE_ERROR "\e[1;91m"
#define STYLE_WARNING "\e[1;93m"
#define STYLE_SUCCESS "\e[92m"
#define STYLE_SUB_SUCCESS "\e[2;92m"
#define STYLE_INFO "\e[2m"
#define STYLE_DEBUG "\e[90m"
#define STYLE_DEBUG_LOC "\e[3;90m"

#ifdef DEBUG
#define KERNEL_INTERNAL_LOG(FMT, ...) \
    logKernelMessage(FMT "\e[m\n" STYLE_DEBUG_LOC " ∟<%s>\t" __FILE__ ":" STRINGX(__LINE__) "\e[m\n" __VA_OPT__(,) __VA_ARGS__, __PRETTY_FUNCTION__);
#else
#define KERNEL_INTERNAL_LOG(FMR, ...) \
    logKernelMessage(FMT "\n" __VA_OPT__(,) __VA_ARGS__);
#endif

#define KERNEL_ERROR(FMT, ...) KERNEL_INTERNAL_LOG(STYLE_ERROR "[!] " FMT __VA_OPT__(,) __VA_ARGS__)
#define KERNEL_WARNING(FMT, ...) KERNEL_INTERNAL_LOG(STYLE_WARNING "[!] " FMT __VA_OPT__(,) __VA_ARGS__)
#define KERNEL_SUCCESS(FMT, ...) KERNEL_INTERNAL_LOG(STYLE_SUCCESS "[+] " FMT __VA_OPT__(,) __VA_ARGS__)
#define KERNEL_SUBSUCCESS(FMT, ...) KERNEL_INTERNAL_LOG(STYLE_SUB_SUCCESS "[>] " FMT __VA_OPT__(,) __VA_ARGS__)
#define KERNEL_DEBUG(FMT, ...) KERNEL_INTERNAL_LOG(STYLE_DEBUG "[?] " FMT __VA_OPT__(,) __VA_ARGS__)
#define KERNEL_DEBUG_LOC(FMT, ...) KERNEL_INTERNAL_LOG(STYLE_DEBUG_LOC "[?] " FMT __VA_OPT__(,) __VA_ARGS__)

SyscallReturn printSyscall(TrapFrame* frame);

#endif
