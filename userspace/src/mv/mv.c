
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "args.h"
#include "list.h"
#include "util.h"

typedef struct {
    const char* prog;
    bool no_clobber;
    bool no_dir;
    bool update;
    bool error;
    char* target;
    List files;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*,
    "mv [options] [-t] <source> <dest>\n"
    "  or:  mv [options] <source>... <directory>\n"
    "  or:  mv [options] -t <directory> <source>..."
, {
    // Options
    ARG_FLAG('n', "no-clobber", {
        context->no_clobber = true;
    }, "do not overwrite an existing file");
    ARG_VALUED('t', "target-directory", {
        free(context->target);
        context->target = strdup(value);
    }, false, "=<directory>", "move all sources into the directory");
    ARG_FLAG('T', "no-target-directory", {
        context->no_dir = true;
    }, "treat the destination as a normal file");
    ARG_FLAG('u', "update", {
        context->update = true;
    }, "move only if the source is newer that the destination");
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

static void moveFromTo(const char* from, const char* to, Arguments* args) {
    struct stat src;
    if (stat(from, &src) != 0) {
        args->error = true;
        fprintf(stderr, "%s: cannot stat '%s': %s\n", args->prog, from, strerror(errno));
        return;
    }
    struct stat dst;
    bool dst_exists = stat(to, &dst) == 0;
    if (
        (!args->no_clobber || !dst_exists)
        && (!args->update || src.st_mtime > dst.st_mtime)
    ) {
        if (dst_exists) {
            if (remove(to) != 0) {
                args->error = true;
                fprintf(stderr, "%s: cannot remove '%s': %s\n", args->prog, to, strerror(errno));
                return;
            }
        }
        if (rename(from, to) != 0) {
            args->error = true;
            fprintf(stderr, "%s: cannot move '%s': %s\n", args->prog, from, strerror(errno));
            return;
        }
    }
}

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    args.no_clobber = false;
    args.no_dir = false;
    args.update = false;
    args.target = NULL;
    args.error = false;
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
    if (is_dir) {
        if (args.no_dir) {
            fprintf(stderr, "%s: target '%s': %s\n", args.prog, args.target, strerror(EEXIST));
            exit(1);
        }
        for (size_t i = 0; i < args.files.count; i++) {
            char* path = LIST_GET(char*, args.files, i);
            char* new_path = joinPaths(args.target, basename(path));
            moveFromTo(path, new_path, &args);
            free(new_path);
        }
    } else {
        if (args.files.count > 1) {
            fprintf(stderr, "%s: target '%s': %s\n", args.prog, args.target, strerror(ENOTDIR));
            exit(1);
        }
        char* path = LIST_GET(char*, args.files, 0);
        moveFromTo(path, args.target, &args);
    }
    deinitListAndContents(&args.files);
    free(args.target);
    return args.error ? 1 : 0;
}

