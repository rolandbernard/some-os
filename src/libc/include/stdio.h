#ifndef _LIBC_STDIO_H_
#define _LIBC_STDIO_H_

#include <stddef.h>
#include <stdarg.h>

int vsnprintf(char* buf, size_t size, const char* fmt, va_list args);

int snprintf(char* buf, size_t size, const char* fmt, ...);

int vsprintf(char* buf, const char* fmt, va_list args);

int sprintf(char* buf, const char* fmt, ...);

#endif
