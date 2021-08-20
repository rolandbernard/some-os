#ifndef _LOG_H_
#define _LOG_H_

#include "error/error.h"
#include "util/text.h"

// Initialize the log system
Error initLogSystem();

// Log the given message
Error logKernelMessage(const char* fmt, ...);

#define STRING(X) #X
#define STRINGX(X) STRING(X)

#ifdef DEBUG
#define KERNEL_LOG(FMT, ...) { \
    logKernelMessage(FMT "\n \e[2;3m∟ <%s>\t" __FILE__ ":" STRINGX(__LINE__) "\e[m" __VA_OPT__(,) __VA_ARGS__, __PRETTY_FUNCTION__); \
}
#else
#define KERNEL_LOG(FMT, ...) { \
    logKernelMessage(FMT __VA_OPT__(,) __VA_ARGS__); \
}
#endif

#endif
