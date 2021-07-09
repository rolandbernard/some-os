#ifndef _TEXT_H_
#define _TEXT_H_

#define FORMAT_STRING(name, fmt) \
    va_list args; \
    va_start(args, fmt); \
    size_t size = vsnprintf(NULL, 0, fmt, args); \
    va_end(args); \
    va_start(args, fmt); \
    char name[size + 1]; \
    vsnprintf(string, size + 1, fmt, args); \
    va_end(args);

#endif
