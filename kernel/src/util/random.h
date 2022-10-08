#ifndef _UTIL_RANDOM_H_
#define _UTIL_RANDOM_H_

#include <stddef.h>
#include <stdint.h>

#include "memory/virtptr.h"

void addRandomEvent(uint8_t* data, size_t size);

void getRandom(VirtPtr buffer, size_t size);

#endif
