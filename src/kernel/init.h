#ifndef _INIT_H_
#define _INIT_H_

#include "error/error.h"

// Initialize all systems needed to start a process
Error initBasicSystems();

// Initialize all the kernel systems that are not in the basic set
Error initAllSystems();

// Initialize hart. Must run in Machine mode.
void initHart();

// Initialize the hart that is initializing the kernel.
void initPrimaryHart();

void initHarts();

#endif
