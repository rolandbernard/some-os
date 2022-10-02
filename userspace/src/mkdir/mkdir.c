
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
    bool parents;
    bool verbose;
    bool error;
    List files;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "mkdir [options] <directory>...", {
    // Options
    ARG_FLAG('p', "parents", {
        context->parents = true;
    }, "no error if existing and make parent directories");
    ARG_FLAG('v', "verbose", {
        context->verbose = true;
    }, "print a message for each created directory");
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

static bool mkdirPath(const char* path, Arguments* args) {
    struct stat stats;
    bool exists = stat(path, &stats) == 0;
    if (!exists && errno != ENOENT) {
        args->error = true;
        fprintf(stderr, "%s: target '%s': %s\n", args->prog, path, strerror(errno));
        return false;
    }
    if (exists && !args->parents) {
        args->error = true;
        fprintf(stderr, "%s: target '%s': %s\n", args->prog, path, strerror(EEXIST));
        return false;
    } else if (!exists) {
        if (args->parents) {
            char* parent = dirname(path);
            if ((parent[0] != '/' && parent[0] != '.') || parent[1] != 0) {
                if (!mkdirPath(parent, args)) {
                    return false;
                }
            }
            free(parent);
        }
        if (mkdir(path, 0777) != 0) {
            args->error = true;
            fprintf(stderr, "%s: cannot create '%s': %s\n", args->prog, path, strerror(errno));
            return false;
        }
        if (args->verbose) {
            printf("created directory '%s'\n", path);
        }
    }
    return true;
}

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    args.parents = false;
    args.verbose = false;
    args.error = false;
    initList(&args.files);
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    for (size_t i = 0; i < args.files.count; i++) {
        char* path = LIST_GET(char*, args.files, i);
        mkdirPath(path, &args);
        free(path);
    }
    deinitList(&args.files);
    return args.error ? 1 : 0;
}

