#ifndef _DEBUGINFO_H_
#define _DEBUGINFO_H_

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uintptr_t addr;
    const char* symbol;
} SymbolDebugInfo;

typedef struct {
    uintptr_t addr;
    const char* file;
    size_t line;
} LineDebugInfo;

const SymbolDebugInfo* searchSymbolDebugInfo(uintptr_t addr);

const LineDebugInfo* searchLineDebugInfo(uintptr_t addr);

#endif
