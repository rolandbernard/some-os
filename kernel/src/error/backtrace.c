
#include <string.h>
#include <unwind.h>

#include "error/debuginfo.h"
#include "error/log.h"
#include "error/panic.h"
#include "task/syscall.h"

#include "error/backtrace.h"

void __register_frame(const void* begin);

extern char __eh_frame_start[];

static bool initialized = false;

Error initBacktrace() {
    __register_frame(__eh_frame_start);
    initialized = true;
    return simpleError(SUCCESS);
}

static void logFrameReturnAddress(int depth, uintptr_t addr, uintptr_t stack, bool kernel) {
    int indent = depth;
    while (indent < 10000) {
        logKernelMessage(" ");
        indent *= 10;
    }
    logKernelMessage(STYLE_DEBUG "#%d: \e[m" STYLE_INFO "%p\e[m" STYLE_DEBUG "[%p]\e[m", depth, addr, stack);
    if (kernel) {
        const SymbolDebugInfo* symb_info = searchSymbolDebugInfo(addr - 1);
        if (symb_info != NULL) {
            logKernelMessage(STYLE_DEBUG " (\e[m" STYLE_INFO "%s\e[m" STYLE_DEBUG "+%p)\e[m", symb_info->symbol, addr - symb_info->addr);
        }
        const LineDebugInfo* line_info = searchLineDebugInfo(addr - 1);
        if (line_info != NULL) {
            logKernelMessage(STYLE_DEBUG_LOC " %s:%d\e[m", line_info->file, line_info->line);
        }
    }
    logKernelMessage("\e[m\n");
}

typedef struct {
    int depth;
    uintptr_t last_cfa;
    uintptr_t last_addr;
} UnwindTraceData;

static _Unwind_Reason_Code logTracingFunction(struct _Unwind_Context *ctx, void *udata) {
    UnwindTraceData* data = (UnwindTraceData*)udata;
    data->last_cfa = _Unwind_GetCFA(ctx);
    if (data->depth >= 0) {
        if (data->last_addr != 0) {
            logFrameReturnAddress(data->depth + 1, data->last_addr, data->last_cfa, true);
        }
    }
    data->last_addr = _Unwind_GetIP(ctx);
    data->depth++;
    return _URC_NO_REASON;
}

static void backtraceSkipping(_Unwind_Trace_Fn func, int depth) {
    if (!initialized) {
        initBacktrace();
    }
    UnwindTraceData data = {
        .depth = -depth,
    };
    _Unwind_Backtrace(func, &data);
}

void logBacktrace() {
    backtraceSkipping(logTracingFunction, 3);
}

static void magicLogBacktrace() {
    backtraceSkipping(logTracingFunction, 4);
}

// Magic that will allow to unwind the backtrace for the given frame.
void magicCfiIndirect(TrapFrame* frame, void* calling);

void logBacktraceFor(TrapFrame* frame) {
    magicCfiIndirect(frame, magicLogBacktrace);
}

#ifdef PROFILE
static _Unwind_Reason_Code profileTracingFunction(struct _Unwind_Context *ctx, void *udata) {
    UnwindTraceData* data = (UnwindTraceData*)udata;
    if (data->depth >= 0) {
        uintptr_t addr = _Unwind_GetIP(ctx);
        SymbolDebugInfo* symb_info = searchSymbolDebugInfo(addr - 1);
        if (symb_info != NULL) {
            symb_info->profile++;
        }
        LineDebugInfo* line_info = searchLineDebugInfo(addr - 1);
        if (line_info != NULL) {
            line_info->profile++;
        }
    }
    data->depth++;
    return _URC_NO_REASON;
}

static void magicProfileBacktrace() {
    backtraceSkipping(profileTracingFunction, 3);
}

void profileBacktraceFor(TrapFrame* frame) {
    magicCfiIndirect(frame, magicProfileBacktrace);
}
#endif

