
#include "error/debuginfo.h"

// This file is a placeholder. If compiling the debug build, symbol information will be extracted
// from the kernel and a object file generated.

const size_t __attribute__((weak)) __attribute__((section(".rtdebug"))) symbol_debug_count = 0;
const SymbolDebugInfo __attribute__((weak)) __attribute__((section(".rtdebug"))) symbol_debug[0];

const size_t __attribute__((weak)) __attribute__((section(".rtdebug"))) line_debug_count = 0;
const LineDebugInfo __attribute__((weak)) __attribute__((section(".rtdebug"))) line_debug[0];

const SymbolDebugInfo* searchSymbolDebugInfo(uintptr_t addr) {
    if (symbol_debug_count == 0) {
        return NULL;
    } else {
        for (size_t i = 0; i < symbol_debug_count - 1; i++) {
            if (symbol_debug[i + 1].addr > addr) {
                return &symbol_debug[i];
            }
        }
        return &symbol_debug[symbol_debug_count - 1];
    }
}

const LineDebugInfo* searchLineDebugInfo(uintptr_t addr) {
    if (symbol_debug_count == 0) {
        return NULL;
    } else {
        for (size_t i = 0; i < line_debug_count - 1; i++) {
            if (line_debug[i + 1].addr > addr) {
                return &line_debug[i];
            }
        }
        return &line_debug[line_debug_count - 1];
    }
}

