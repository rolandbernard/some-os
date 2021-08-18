#ifndef _LOG_H_
#define _LOG_H_

#include "error/error.h"

// Initialize the log system
Error initLogSystem();

// Log the given message
Error logKernelMessage(const char* fmt, ...);

#endif
