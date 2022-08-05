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

extern size_t symbol_debug_count;
extern SymbolDebugInfo symbol_debug[];

extern size_t line_debug_count;
extern LineDebugInfo line_debug[];

SymbolDebugInfo* searchSymbolDebugInfo(uintptr_t addr);

LineDebugInfo* searchLineDebugInfo(uintptr_t addr);

#endif
