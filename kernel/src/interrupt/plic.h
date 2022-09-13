#ifndef _PLIC_H_
#define _PLIC_H_

#include <stdint.h>

#include "error/error.h"

typedef uint8_t InterruptPriority;
typedef uint32_t ExternalInterrupt;
typedef void (*ExternalInterruptFunction)(ExternalInterrupt id, void* udata);

void handleExternalInterrupt();

void setInterruptFunction(ExternalInterrupt id, ExternalInterruptFunction function, void* udata);

void clearInterruptFunction(ExternalInterrupt id, ExternalInterruptFunction function, void* udata);

void enableInterrupt(ExternalInterrupt id);

void disableInterrupt(ExternalInterrupt id);

void setInterruptPriority(ExternalInterrupt id, InterruptPriority priority);

void setPlicPriorityThreshold(InterruptPriority priority);

ExternalInterrupt nextInterrupt();

void completeInterrupt(ExternalInterrupt id);

Error registerDriverPlic();

#endif
