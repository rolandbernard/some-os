#ifndef _DEBUGINFO_H_
#define _DEBUGINFO_H_

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uintptr_t addr;
    const char* symbol;
#ifdef PROFILE
    size_t profile;
#endif
} SymbolDebugInfo;

typedef struct {
    uintptr_t addr;
    const char* file;
    size_t line;
#ifdef PROFILE
    size_t profile;
#endif
} LineDebugInfo;

SymbolDebugInfo* searchSymbolDebugInfo(uintptr_t addr);

LineDebugInfo* searchLineDebugInfo(uintptr_t addr);

#endif
