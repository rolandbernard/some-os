#ifndef _VFS_TYPES_H_
#define _VFS_TYPES_H_

#include <stddef.h>
#include <stdint.h>

#include "devices/devices.h"
#include "error/error.h"
#include "interrupt/timer.h"
#include "memory/virtptr.h"
#include "task/tasklock.h"

typedef enum {
    VFS_TYPE_UNKNOWN = 0,
    VFS_TYPE_FIFO = 1,
    VFS_TYPE_CHAR = 2,
    VFS_TYPE_DIR = 4,
    VFS_TYPE_BLOCK = 6,
    VFS_TYPE_REG = 8,
    VFS_TYPE_LNK = 10,
    VFS_TYPE_SOCK = 12,
    VFS_TYPE_MT = 15,
} VfsFileType;

typedef enum {
    VFS_SEEK_CUR = 0,
    VFS_SEEK_SET = 1,
    VFS_SEEK_END = 2,
} VfsSeekWhence;

typedef enum {
    VFS_OPEN_CREATE = (1 << 0),
    VFS_OPEN_APPEND = (1 << 1),
    VFS_OPEN_TRUNC = (1 << 2),
    VFS_OPEN_DIRECTORY = (1 << 3),
    VFS_OPEN_READ = (1 << 4),
    VFS_OPEN_WRITE = (1 << 5),
    VFS_OPEN_EXECUTE = (1 << 6),
    VFS_OPEN_REGULAR = (1 << 7),
    VFS_OPEN_CLOEXEC = (1 << 8),
    VFS_OPEN_EXCL = (1 << 9),
    VFS_OPEN_RDONLY = (1 << 10),
    VFS_OPEN_WRONLY = (1 << 11),
    VFS_OPEN_NOFOLLOW = (1 << 12), // TODO: implement symbolic links
} VfsOpenFlags;

typedef enum {
    VFS_UNLINK_REMOVEDIR = (1 << 0),
} VfsUnlinkFlags;

#define OPEN_ACCESS(open_flags) ((open_flags >> 4) & 0b111)

typedef enum {
    VFS_ACCESS_R = (1 << 0),
    VFS_ACCESS_W = (1 << 1),
    VFS_ACCESS_X = (1 << 2),
    VFS_ACCESS_REG = (1 << 3),
    VFS_ACCESS_DIR = (1 << 4),
} VfsAccessFlags;

typedef enum {
    VFS_MODE_A_X = (1 << 0),
    VFS_MODE_A_W = (1 << 1),
    VFS_MODE_A_R = (1 << 2),
    VFS_MODE_G_X = (1 << 3),
    VFS_MODE_G_W = (1 << 4),
    VFS_MODE_G_R = (1 << 5),
    VFS_MODE_O_X = (1 << 6),
    VFS_MODE_O_W = (1 << 7),
    VFS_MODE_O_R = (1 << 8),
    VFS_MODE_STICKY = (1 << 9),
    VFS_MODE_SETUID = (1 << 10),
    VFS_MODE_SETGID = (1 << 11),
    VFS_MODE_TYPE = (0xf << 12),
} VfsModeFlags;

typedef enum {
    VFS_FILE_CLOEXEC = (1 << 0),
    VFS_FILE_RDONLY = (1 << 1),
    VFS_FILE_WRONLY = (1 << 2),
} VfsFileDescFlags;

typedef int VfsFileDescId;

#define VFS_MODE_A_RW (VFS_MODE_A_R | VFS_MODE_A_W)
#define VFS_MODE_G_RW (VFS_MODE_G_R | VFS_MODE_G_W)
#define VFS_MODE_O_RW (VFS_MODE_O_R | VFS_MODE_O_W)
#define VFS_MODE_OG_RW (VFS_MODE_O_RW | VFS_MODE_G_RW)
#define VFS_MODE_OGA_RW (VFS_MODE_OG_RW | VFS_MODE_A_RW)

#define MODE_TYPE(mode) (mode >> 12)
#define TYPE_MODE(type) (type << 12)

typedef uint16_t VfsMode;
typedef int Uid;
typedef int Gid;

typedef struct {
    size_t id;
    VfsMode mode;
    size_t nlinks;
    Uid uid;
    Gid gid;
    size_t size;
    size_t block_size;
    Time st_atime;
    Time st_mtime;
    Time st_ctime;
    size_t dev;
} VfsStat;

typedef struct {
    size_t id;
    size_t off;
    size_t len;
    VfsFileType type;
    char name[];
} VfsDirectoryEntry;

struct VfsSuperblock_s;
struct VfsNode_s;
struct VfsFile_s;

typedef void (*VfsSuperblockFreeFunction)(struct VfsSuperblock_s* sb);
typedef Error (*VfsSuperblockReadNodeFunction)(struct VfsSuperblock_s* sb, size_t id, struct VfsNode_s** out);
typedef Error (*VfsSuperblockWriteNodeFunction)(struct VfsSuperblock_s* sb, struct VfsNode_s* write);
typedef Error (*VfsSuperblockNewNodeFunction)(struct VfsSuperblock_s* sb, struct VfsNode_s** out);
typedef Error (*VfsSuperblockFreeNodeFunction)(struct VfsSuperblock_s* sb, struct VfsNode_s* free);

typedef struct {
    VfsSuperblockFreeFunction free;             // Free all data for the vfs superblock.
    VfsSuperblockReadNodeFunction read_node;    // Read information about the given node id.
    VfsSuperblockWriteNodeFunction write_node;  // Write back the information about the given node.
    VfsSuperblockNewNodeFunction new_node;      // Create a new node, with a new id.
    VfsSuperblockFreeNodeFunction free_node;    // Remove the node from the filesystem.
} VfsSuperblockFunctions;

typedef struct VfsSuperblock_s {
    VfsSuperblockFunctions* functions;
    struct VfsNode_s* mount_point;
    struct VfsNode_s* root_node;
    size_t id;
    size_t ref_count;
    TaskLock lock;
    // TODO: Add node cache!
} VfsSuperblock;

typedef void (*VfsNodeFreeFunction)(struct VfsNode_s* node);
typedef Error (*VfsNodeReadAtFunction)(struct VfsNode_s* node, VirtPtr buff, size_t offset, size_t length, size_t* read);
typedef Error (*VfsNodeWriteAtFunction)(struct VfsNode_s* node, VirtPtr buff, size_t offset, size_t length, size_t* written);
typedef Error (*VfsNodeReaddirAtFunction)(struct VfsNode_s* node, VirtPtr buff, size_t offset, size_t length, size_t* read);
typedef Error (*VfsNodeTruncFunction)(struct VfsNode_s* node, size_t length);
typedef Error (*VfsNodeLookupFunction)(struct VfsNode_s* node, const char* name, size_t* node_id);
typedef Error (*VfsNodeUnlinkFunction)(struct VfsNode_s* node, const char* name);
typedef Error (*VfsNodeLinkFunction)(struct VfsNode_s* node, const char* name, struct VfsNode_s* entry);

typedef struct {
    VfsNodeFreeFunction free;               // Free all information for the vfs node.
    VfsNodeReadAtFunction read_at;          // Read length bytes from file at offset into buffer.
    VfsNodeWriteAtFunction write_at;        // Write length bytes from buffer info file at offset.
    VfsNodeTruncFunction trunc;             // Truncate this file to have size length.
    VfsNodeReaddirAtFunction readdir_at;    // If this is a directory, read the entry starting at offset.
    VfsNodeLookupFunction lookup;           // If this is a directory, find the node id of the entry with name.
    VfsNodeUnlinkFunction unlink;           // If this is a directory, remove the entry with name.
    VfsNodeLinkFunction link;               // If this is a directory, add entry at name.
} VfsNodeFunctions;

typedef struct VfsNode_s {
    VfsNodeFunctions* functions;
    VfsSuperblock* superblock;
    size_t id;
    size_t nlinks;
    VfsMode mode;
    Uid uid;
    Gid gid;
    size_t size;
    Time st_atime;
    Time st_mtime;
    Time st_ctime;
    size_t ref_count;
    TaskLock lock;
    Device* device;         // If this node represents a device, this is not NULL. (Also, functions should be changed.)
    VfsSuperblock* mounted; // If a filesystem is mounted at this node, this is not NULL.
} VfsNode;

struct PipeSharedData_s;

typedef struct VfsFile_s {
    VfsNode* node;
    char* path;
    size_t ref_count;
    size_t offset;
    TaskLock lock;
} VfsFile;

typedef struct VfsFileDescriptor_s {
    struct VfsFileDescriptor_s* next;
    VfsFile* file;
    VfsFileDescId id;
    VfsFileDescFlags flags;
} VfsFileDescriptor;

typedef struct {
    VfsSuperblock* root_mounted;
    TaskLock lock;
} VirtualFilesystem;

#endif
