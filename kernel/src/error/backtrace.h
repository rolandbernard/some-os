#ifndef _BACKTRACE_H_
#define _BACKTRACE_H_

#include "error/error.h"
#include "task/types.h"

Error initBacktrace();

void logBacktrace();

void logBacktraceFor(TrapFrame* frame);

#ifdef PROFILE
void profileBacktraceFor(TrapFrame* frame);
#endif

#endif
