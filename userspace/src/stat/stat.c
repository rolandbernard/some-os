
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "args.h"
#include "list.h"
#include "util.h"

typedef struct {
    const char* prog;
    bool terse;
    bool error;
    List files;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "stat [options] <file>...", {
    // Options
    ARG_FLAG('t', "terse", {
        context->terse = true;
    }, "print the information in terse form");
    ARG_FLAG(0, "help", {
        ARG_PRINT_HELP(argumentSpec, NULL);
        exit(0);
    }, "display this help and exit");
}, {
    // Default
    copyStringToList(&context->files, value);
}, {
    // Warning
    if (option != NULL) {
        fprintf(stderr, "%s: '%s': %s\n", argv[0], option, warning);
    } else {
        fprintf(stderr, "%s: %s\n", argv[0], warning);
    }
    exit(2);
}, {
    // Final
    if (context->files.count == 0) {
        const char* option = NULL;
        ARG_WARN("missing file operand");
    }
})

static const char* modeTypeString(mode_t mode) {
    switch (mode & S_IFMT) {
        case S_IFBLK:
            return "block device";
        case S_IFCHR:
            return "character device";
        case S_IFDIR:
            return "directory";
        case S_IFIFO:
            return "fifo";
        case S_IFLNK:
            return "symbolic link";
        case S_IFREG:
            return "regular file";
        case S_IFSOCK:
            return "socket";
        default:
            return "unknown";
    }
}

static void statFile(const char* path, Arguments* args) {
    struct stat stats;
    if (stat(path, &stats) != 0) {
        args->error = true;
        fprintf(stderr, "%s: cannot stat '%s': %s\n", args->prog, path, strerror(errno));
        return;
    }
    if (args->terse) {
        printf(
            "%s %lu %lu %.4x %u %u %.4x %i %i 0 0 %lu %lu %lu %lu %lu\n", path, stats.st_size,
            stats.st_blocks, stats.st_mode & 0xffff, stats.st_uid, stats.st_gid, stats.st_dev,
            stats.st_ino, stats.st_nlink, stats.st_atime, stats.st_mtime, stats.st_ctime,
            stats.st_ctime, stats.st_blksize
        );
    } else {
        char mode[10];
        if ((stats.st_mode & S_IFMT) == S_IFBLK) {
            mode[0] = 'b';
        } else if ((stats.st_mode & S_IFMT) == S_IFCHR) {
            mode[0] = 'c';
        } else if ((stats.st_mode & S_IFMT) == S_IFDIR) {
            mode[0] = 'd';
        } else if ((stats.st_mode & S_IFMT) == S_IFIFO) {
            mode[0] = 'f';
        } else if ((stats.st_mode & S_IFMT) == S_IFLNK) {
            mode[0] = 'l';
        } else {
            mode[0] = '-';
        }
        for (size_t i = 0; i < 3; i++) {
            mode_t grp = (stats.st_mode >> (3 * i));
            mode[9 - 3 * i] = (grp & 0b001) != 0 ? 'x' : '-';
            mode[8 - 3 * i] = (grp & 0b010) != 0 ? 'w' : '-';
            mode[7 - 3 * i] = (grp & 0b100) != 0 ? 'r' : '-';
        }
        if ((stats.st_mode & S_ISUID) != 0) {
            mode[3] = mode[3] == 'x' ? 's' : 'S';
        }
        if ((stats.st_mode & S_ISGID) != 0) {
            mode[6] = mode[6] == 'x' ? 's' : 'S';
        }
        if ((stats.st_mode & S_ISVTX) != 0) {
            mode[9] = mode[9] == 'x' ? 't' : 'T';
        }
        printf("  File: %s\n", path);
        printf(
            "  Size: %lu   Blocks: %lu   IO Block: %lu   %s\n", stats.st_size, stats.st_blocks,
            stats.st_blksize, modeTypeString(stats.st_mode)
        );
        printf("Device: %i   Inode: %i   Links: %i\n", stats.st_dev, stats.st_ino, stats.st_nlink);
        printf("Access: (%.4o/%s)   Uid: %i   Gid: %i\n", stats.st_mode & 07777, mode, stats.st_uid, stats.st_gid);
        char time[256];
        const struct tm* tm = localtime(&stats.st_atime);
        strftime(time, sizeof(time), "%F %T", tm);
        printf("Access: %s.%.9lu\n", time, stats.st_atim.tv_nsec);
        tm = localtime(&stats.st_mtime);
        strftime(time, sizeof(time), "%F %T", tm);
        printf("Modify: %s.%.9lu\n", time, stats.st_mtim.tv_nsec);
        tm = localtime(&stats.st_ctime);
        strftime(time, sizeof(time), "%F %T", tm);
        printf("Change: %s.%.9lu\n", time, stats.st_ctim.tv_nsec);
    }
}

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    args.terse = false;
    args.error = false;
    initList(&args.files);
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    for (size_t i = 0; i < args.files.count; i++) {
        char* path = LIST_GET(char*, args.files, i);
        statFile(path, &args);
        free(path);
    }
    deinitList(&args.files);
    return args.error ? 1 : 0;
}

