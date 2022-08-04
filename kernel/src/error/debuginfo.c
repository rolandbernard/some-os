
#include "error/debuginfo.h"

// This file is a placeholder. If compiling the debug build, symbol information will be extracted
// from the kernel and a object file generated.

size_t __attribute__((weak)) __attribute__((section(".rtdebug"))) symbol_debug_count = 0;

SymbolDebugInfo __attribute__((weak)) __attribute__((section(".rtdebug"))) symbol_debug[0];

SymbolDebugInfo* searchSymbolDebugInfo(uintptr_t addr) {
    return NULL;
}

