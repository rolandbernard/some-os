#ifndef _INIT_H_
#define _INIT_H_

#include "error/error.h"

// Initialize all the kernel systems
Error initAllSystems();

// Initialize hart. Must run in Machine mode.
void initHart();

void initHarts();

#endif
