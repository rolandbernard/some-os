#ifndef _CLINT_H_
#define _CLINT_H_

#include "error/error.h"
#include "kernel/time.h"

void sendMachineSoftwareInterrupt(int hart);

void clearMachineSoftwareInterrupt(int hart);

Time getTime();

void setTimeCmp(Time time);

Error registerDriverClint();

#endif
