#ifndef _TEXT_H_
#define _TEXT_H_

#include <stddef.h>
#include <stdio.h>

#define FORMAT_STRINGV(NAME, FMT, ARGS) \
    va_list ARGS ## _copy; \
    va_copy(ARGS ## _copy, ARGS); \
    size_t _ ## NAME ## _size = vsnprintf(NULL, 0, FMT, ARGS ## _copy); \
    va_end(ARGS ## _copy); \
    va_copy(ARGS ## _copy, ARGS); \
    char NAME[_ ## NAME ## _size + 1]; \
    vsnprintf(NAME, _ ## NAME ## _size + 1, FMT, ARGS ## _copy); \
    va_end(ARGS ## _copy);

#define FORMAT_STRING(NAME, FMT) \
    va_list _ ## NAME ## _args; \
    va_start(_ ## NAME ## _args, FMT); \
    FORMAT_STRINGV(NAME, FMT, _ ## NAME ## _args); \
    va_end(_ ## NAME ## _args);

#define FORMAT_STRINGX(NAME, FMT, ...) \
    size_t _ ## NAME ## _size = snprintf(NULL, 0, FMT __VA_OPT__(,) __VA_ARGS__); \
    char NAME[_ ## NAME ## _size + 1]; \
    snprintf(NAME, _ ## NAME ## _size + 1, FMT __VA_OPT__(,) __VA_ARGS__); \

#endif
