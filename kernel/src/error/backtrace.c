
#include <unwind.h>

#include "error/backtrace.h"
#include "memory/kalloc.h"

#include "error/log.h"

void __register_frame(const void* begin);

extern char __eh_frame_start[];

static bool initialized = false;

Error initBacktrace() {
    __register_frame(__eh_frame_start);
    initialized = true;
    return simpleError(SUCCESS);
}

_Unwind_Reason_Code unwindTracingFunction(struct _Unwind_Context *ctx, void *d) {
    int *depth = (int*)d;
    logKernelMessage("    \e[2;3m#%d: %p\e[m\n", *depth, _Unwind_GetIP(ctx));
    (*depth)++;
    return _URC_NO_REASON;
}

void logBacktrace() {
    if (initialized) {
        int depth = 0;
        _Unwind_Backtrace(&unwindTracingFunction, &depth);
    }
}

