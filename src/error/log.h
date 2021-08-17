#ifndef _LOG_H_
#define _LOG_H_

#include "error/error.h"

Error initLogSystem();

Error logKernelMessage(const char* fmt, ...);

#endif
