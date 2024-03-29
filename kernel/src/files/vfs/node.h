#ifndef _VFS_NODE_H_
#define _VFS_NODE_H_

#include "files/vfs/types.h"
#include "process/types.h"

Error vfsNodeReadAt(VfsNode* node, Process* process, VirtPtr buff, size_t offset, size_t length, size_t* read, bool block);

Error vfsNodeWriteAt(VfsNode* node, Process* process, VirtPtr buff, size_t offset, size_t length, size_t* written, bool block);

Error vfsNodeReaddirAt(VfsNode* node, Process* process, VirtPtr buff, size_t offset, size_t length, size_t* read_file, size_t* written_buff);

Error vfsNodeTrunc(VfsNode* node, Process* process, size_t length);

Error vfsNodeLookup(VfsNode* node, Process* process, const char* name, VfsNode** ret);

Error vfsNodeUnlink(VfsNode* node, Process* process, const char* name, VfsNode* entry);

Error vfsNodeLink(VfsNode* node, Process* process, const char* name, VfsNode* entry);

Error vfsNodeIoctl(VfsNode* node, Process* process, size_t request, VirtPtr argp, uintptr_t* out);

bool vfsNodeIsReady(VfsNode* node, Process* process, bool write);

void vfsNodeCopy(VfsNode* node);

void vfsNodeClose(VfsNode* node);

#endif
