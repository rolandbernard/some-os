#ifndef _BLOCK_CACHE_H_
#define _BLOCK_CACHE_H_

#include "devices/devices.h"

BlockDevice* wrapBlockDeviceWithCache(BlockDevice* uncached);

#endif
