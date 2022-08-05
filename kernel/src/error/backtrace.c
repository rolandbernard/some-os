
#include <unwind.h>

#include "error/backtrace.h"

#include "error/log.h"
#include "error/debuginfo.h"

void __register_frame(const void* begin);

extern char __eh_frame_start[];

static bool initialized = false;

Error initBacktrace() {
    __register_frame(__eh_frame_start);
    initialized = true;
    return simpleError(SUCCESS);
}

static void logStackFrameForAddress(int depth, uintptr_t addr) {
    logKernelMessage("    \e[2;3m#%d: %p", depth, addr);
    LineDebugInfo* line_info = searchLineDebugInfo(addr);
    if (line_info != NULL) {
        logKernelMessage(" %s:%d", line_info->file, line_info->line);
    }
    SymbolDebugInfo* symb_info = searchSymbolDebugInfo(addr);
    if (symb_info != NULL) {
        logKernelMessage(" (%s+%p)", symb_info->symbol, addr - symb_info->addr);
    }
    logKernelMessage("\e[m\n");
}

_Unwind_Reason_Code unwindTracingFunction(struct _Unwind_Context *ctx, void *d) {
    int* depth = (int*)d;
    if (*depth >= 2) {
        logStackFrameForAddress(*depth - 2, _Unwind_GetIP(ctx) - 4);
    }
    (*depth)++;
    return _URC_NO_REASON;
}

void logBacktrace() {
    if (initialized) {
        int depth = 0;
        _Unwind_Backtrace(&unwindTracingFunction, &depth);
    }
}

