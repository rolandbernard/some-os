#ifndef _TEXT_H_
#define _TEXT_H_

#include <stddef.h>
#include <stdio.h>

#define FORMAT_STRING(NAME, FMT) \
    va_list args; \
    va_start(args, FMT); \
    size_t __ ## NAME ## _size = vsnprintf(NULL, 0, FMT, args); \
    va_end(args); \
    va_start(args, FMT); \
    char NAME[__ ## NAME ## _size + 1]; \
    vsnprintf(NAME, __ ## NAME ## _size + 1, FMT, args); \
    va_end(args);

#define FORMAT_STRINGX(NAME, FMT, ...) \
    size_t __ ## NAME ## _size = snprintf(NULL, 0, FMT __VA_OPT__(,) __VA_ARGS__); \
    char NAME[__ ## NAME ## _size + 1]; \
    snprintf(NAME, __ ## NAME ## _size + 1, FMT __VA_OPT__(,) __VA_ARGS__); \

#endif
