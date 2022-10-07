
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <time.h>
#include <unistd.h>

#include "args.h"
#include "list.h"
#include "util.h"

typedef struct {
    const char* prog;
    bool force;
    bool no_dir;
    bool symbolic;
    bool verbose;
    bool error;
    char* target;
    List files;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*,
    "ln [options] [-T] <source> <dest>\n"
    "  or:  ln [options] <source>... <directory>\n"
    "  or:  ln [options] -t <directory> <source>..."
, {
    // Options
    ARG_FLAG('f', "force", {
        context->force = true;
    }, "remove existing destination files");
    ARG_FLAG('s', "symbolic", {
        context->symbolic = true;
    }, "make symbolic links instead of hard links");
    ARG_VALUED('t', "target-directory", {
        free(context->target);
        context->target = strdup(value);
    }, false, "=<directory>", "copy all sources into the directory");
    ARG_FLAG('T', "no-target-directory", {
        context->no_dir = true;
    }, "treat the destination as a normal file");
    ARG_FLAG('v', "verbose", {
        context->verbose = true;
    }, "explain what is being done");
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
    if (context->target == NULL && context->files.count > 0) {
        context->files.count--;
        context->target = context->files.values[context->files.count];
    }
    if (context->files.count == 0) {
        const char* option = NULL;
        ARG_WARN("missing file operand");
    } else if (context->no_dir && context->files.count > 1) {
        const char* option = context->files.values[1];
        ARG_WARN("extra operand");
    }
})

__attribute__((weak)) int symlink(const char* src, const char* dst) {
    errno = ENOSYS;
    return -1;
}

static void linkPath(const char* src, const char* dst, Arguments* args) {
    struct stat src_stat;
    if (stat(src, &src_stat) != 0) {
        args->error = true;
        fprintf(stderr, "%s: cannot stat '%s': %s\n", args->prog, src, strerror(errno));
        return;
    }
    struct stat dst_stat;
    bool dst_exists = stat(dst, &dst_stat) == 0;
    if (dst_exists) {
        if (args->force) {
            if (remove(dst) != 0) {
                args->error = true;
                fprintf(stderr, "%s: cannot remove '%s': %s\n", args->prog, dst, strerror(errno));
                return;
            }
        } else {
            args->error = true;
            fprintf(stderr, "%s: cannot link at '%s': %s\n", args->prog, dst, strerror(EEXIST));
            return;
        }
    }
    if ((args->symbolic ? symlink(src, dst) : link(src, dst)) != 0) {
        args->error = true;
        fprintf(
            stderr, "%s: cannot link '%s' at '%s': %s\n", args->prog, src, dst, strerror(errno)
        );
        return;
    }
    if (args->verbose) {
        printf("linked '%s' at '%s'\n", src, dst);
    }
}

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    args.force = false;
    args.no_dir = false;
    args.symbolic = false;
    args.verbose = false;
    args.error = false;
    args.target = NULL;
    initList(&args.files);
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    bool is_dir = false;
    struct stat dst;
    if (stat(args.target, &dst) != 0) {
        if (errno != ENOENT && errno != ENOTDIR) {
            fprintf(stderr, "%s: target '%s': %s\n", args.prog, args.target, strerror(errno));
            exit(1);
        }
    } else {
        is_dir = (dst.st_mode & S_IFMT) == S_IFDIR;
    }
    if (is_dir && !args.no_dir) {
        for (size_t i = 0; i < args.files.count; i++) {
            char* path = LIST_GET(char*, args.files, i);
            char* new_path = joinPaths(args.target, basename(path));
            linkPath(path, new_path, &args);
            free(new_path);
        }
    } else {
        if (args.files.count > 1) {
            fprintf(stderr, "%s: target '%s': %s\n", args.prog, args.target, strerror(ENOTDIR));
            exit(1);
        }
        char* path = LIST_GET(char*, args.files, 0);
        linkPath(path, args.target, &args);
    }
    deinitListAndContents(&args.files);
    free(args.target);
    return args.error ? 1 : 0;
}

