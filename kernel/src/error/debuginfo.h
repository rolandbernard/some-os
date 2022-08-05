#ifndef _DEBUGINFO_H_
#define _DEBUGINFO_H_

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uintptr_t addr;
    const char* symbol;
} SymbolDebugInfo;

extern size_t symbol_debug_count;
extern SymbolDebugInfo symbol_debug[];

SymbolDebugInfo* searchSymbolDebugInfo(uintptr_t addr);

#endif
