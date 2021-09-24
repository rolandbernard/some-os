#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <stdint.h>
#include <stdnoreturn.h>

#include "libstart/syscall_values.h"

#define CONSOME(...)
#define KEEP(...) __VA_ARGS__

#define IFN(...) CONSOME __VA_OPT__(()KEEP)

#define IFE(...) KEEP __VA_OPT__(()CONSOME)

#define Z6(...) IFE(__VA_ARGS__)(0, 0, 0, 0, 0, 0) IFN(__VA_ARGS__)(Z5(__VA_ARGS__))
#define Z5(A1, ...) A1, IFE(__VA_ARGS__)(0, 0, 0, 0, 0) IFN(__VA_ARGS__)(Z4(__VA_ARGS__))
#define Z4(A1, ...) A1, IFE(__VA_ARGS__)(0, 0, 0, 0) IFN(__VA_ARGS__)(Z3(__VA_ARGS__))
#define Z3(A1, ...) A1, IFE(__VA_ARGS__)(0, 0, 0) IFN(__VA_ARGS__)(Z2(__VA_ARGS__))
#define Z2(A1, ...) A1, IFE(__VA_ARGS__)(0, 0) IFN(__VA_ARGS__)(Z1(__VA_ARGS__))
#define Z1(A1, ...) A1, IFE(__VA_ARGS__)(0) IFN(__VA_ARGS__)(__VA_ARGS__)

#define SYSCALL(KIND, ...) make_syscall(KIND, Z6(__VA_ARGS__))

uintptr_t make_syscall(
    uintptr_t kind, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5,
    uintptr_t a6
);

void syscall_print(const char* string);

noreturn void syscall_exit(int status);

void syscall_yield();

int syscall_fork();

void syscall_sleep(uint64_t nanoseconds);

#endif
