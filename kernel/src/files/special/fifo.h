#ifndef _FIFO_H_
#define _FIFO_H_

// For now we are reusing the pipe implementation 

#include "files/special/pipe.h"

VfsFile* createFifoFile(VfsNode* node, char* path, bool for_write);

#endif
