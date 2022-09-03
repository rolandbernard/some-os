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
    VFS_TYPE_LNK = 10, // TODO: implement symbolic links
    VFS_TYPE_SOCK = 12,
    VFS_TYPE_MT = 15,
} VfsFileType;

typedef enum {
    VFS_SEEK_SET = 0,
    VFS_SEEK_CUR = 1,
    VFS_SEEK_END = 2,
} VfsSeekWhence;

typedef enum {
    VFS_OPEN_READ        =   0x0001,
    VFS_OPEN_WRITE       =   0x0002,
    VFS_OPEN_ACCESS_MODE =   0x0003,
    VFS_OPEN_APPEND      =   0x0008,
    VFS_OPEN_CREAT       =   0x0200,
    VFS_OPEN_TRUNC       =   0x0400,
    VFS_OPEN_EXCL        =   0x0800,
    VFS_OPEN_CLOEXEC     =  0x40000,
    VFS_OPEN_EXECUTE     = 0x100000,
    VFS_OPEN_DIRECTORY   = 0x200000,
    VFS_OPEN_REGULAR     = 0x400000,
} VfsOpenFlags;

#define OPEN_ACCESS(open_flags) ((open_flags & 0b11) | (((open_flags >> 20) & 0b111) << 2))

typedef enum {
    VFS_ACCESS_R = (1 << 0),
    VFS_ACCESS_W = (1 << 1),
    VFS_ACCESS_X = (1 << 2),
    VFS_ACCESS_DIR = (1 << 3),
    VFS_ACCESS_REG = (1 << 4),
    VFS_ACCESS_CHMOD = (1 << 5),
    VFS_ACCESS_CHOWN = (1 << 6),
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
    VFS_FILE_READ = (1 << 0),
    VFS_FILE_WRITE = (1 << 1),
} VfsFileFlags;

typedef enum {
    VFS_DESC_CLOEXEC = (1 << 0),
} VfsDescFlags;

typedef enum {
    VFS_FCNTL_DUPFD = 0,
    VFS_FCNTL_GETFD = 1,
    VFS_FCNTL_SETFD = 2,
    VFS_FCNTL_GETFL = 3,
    VFS_FCNTL_SETFL = 4,
    VFS_FCNTL_DUPFD_CLOEXEC = 14,
} VfsFcntlRequest;

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
    size_t dev;
    size_t id;
    VfsMode mode;
    size_t nlinks;
    Uid uid;
    Gid gid;
    size_t rdev;
    size_t size;
    size_t block_size;
    size_t blocks;
    Time atime;
    Time mtime;
    Time ctime;
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
typedef Error (*VfsSuperblockNewNodeFunction)(struct VfsSuperblock_s* sb, size_t* id);
typedef Error (*VfsSuperblockFreeNodeFunction)(struct VfsSuperblock_s* sb, struct VfsNode_s* free);

typedef struct {
    VfsSuperblockFreeFunction free;             // Free all data for the vfs superblock.
    VfsSuperblockReadNodeFunction read_node;    // Read information about the given node id.
    VfsSuperblockWriteNodeFunction write_node;  // Write back the information about the given node.
    VfsSuperblockNewNodeFunction new_node;      // Create a new node, with a new id.
    VfsSuperblockFreeNodeFunction free_node;    // Remove the node from the filesystem.
} VfsSuperblockFunctions;

typedef struct {
    struct VfsNode_s** nodes;
    size_t count;
    size_t capacity;
    TaskLock lock;
} VfsNodeCache;

typedef struct VfsSuperblock_s {
    const VfsSuperblockFunctions* functions;
    struct VfsNode_s* root_node;
    size_t id;
    size_t ref_count;
    TaskLock lock;
    VfsNodeCache nodes;
} VfsSuperblock;

typedef void (*VfsNodeFreeFunction)(struct VfsNode_s* node);
typedef Error (*VfsNodeReadAtFunction)(struct VfsNode_s* node, VirtPtr buff, size_t offset, size_t length, size_t* read);
typedef Error (*VfsNodeWriteAtFunction)(struct VfsNode_s* node, VirtPtr buff, size_t offset, size_t length, size_t* written);
typedef Error (*VfsNodeReaddirAtFunction)(struct VfsNode_s* node, VirtPtr buff, size_t offset, size_t length, size_t* read_file, size_t* written_buff);
typedef Error (*VfsNodeTruncFunction)(struct VfsNode_s* node, size_t length);
typedef Error (*VfsNodeLookupFunction)(struct VfsNode_s* node, const char* name, size_t* node_id);
typedef Error (*VfsNodeUnlinkFunction)(struct VfsNode_s* node, const char* name);
typedef Error (*VfsNodeLinkFunction)(struct VfsNode_s* node, const char* name, struct VfsNode_s* entry);
typedef Error (*VfsNodeIoctlFunction)(struct VfsNode_s* node, size_t request, VirtPtr argp, int* out);

typedef struct {
    VfsNodeFreeFunction free;               // Free all information for the vfs node.
    VfsNodeReadAtFunction read_at;          // Read length bytes from file at offset into buffer.
    VfsNodeWriteAtFunction write_at;        // Write length bytes from buffer info file at offset.
    VfsNodeTruncFunction trunc;             // Truncate this file to have size length.
    VfsNodeReaddirAtFunction readdir_at;    // If this is a directory, read the entry starting at offset.
    VfsNodeLookupFunction lookup;           // If this is a directory, find the node id of the entry with name.
    VfsNodeUnlinkFunction unlink;           // If this is a directory, remove the entry with name.
    VfsNodeLinkFunction link;               // If this is a directory, add entry at name.
    VfsNodeIoctlFunction ioctl;
} VfsNodeFunctions;

typedef struct VfsNode_s {
    const VfsNodeFunctions* functions;
    VfsSuperblock* superblock;
    VfsStat stat;
    size_t ref_count;
    TaskLock lock;
    VfsSuperblock* mounted; // If a filesystem is mounted at this node, this is not NULL.
    struct VfsNode_s* real_node; // node->real_node != node if node is a special file node (pipe/fifo/block/tty).
} VfsNode;

struct PipeSharedData_s;

typedef struct VfsFile_s {
    VfsNode* node;
    char* path;
    size_t ref_count;
    size_t offset;
    VfsFileFlags flags;
    TaskLock lock;
} VfsFile;

typedef struct VfsFileDescriptor_s {
    struct VfsFileDescriptor_s* next;
    VfsFile* file;
    VfsFileDescId id;
    VfsDescFlags flags;
    size_t ref_count;
} VfsFileDescriptor;

typedef struct VfsTmpMounts_s {
    struct VfsTmpMounts_s* next;
    char* prefix;
    VfsSuperblock* mounted;
} VfsTmpMount;

typedef struct {
    VfsSuperblock* root_mounted;
    VfsTmpMount* tmp_mounted;
    TaskLock lock;
} VirtualFilesystem;

#endif
