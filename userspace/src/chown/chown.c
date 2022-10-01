
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
    bool recursive;
    bool error;
    char* reference;
    List files;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*,
    "chown [options] <uid>[:<gid>] <file>...\n"
    "  or:  chown [options] --reference=<file> <file>..."
, {
    // Options
    ARG_VALUED(0, "reference", {
        context->reference = strdup(value);
    }, false, "=<file>", "use the given files owner");
    ARG_FLAG('R', "recursive", {
        context->recursive = true;
    }, "change files and directories recursively");
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
    if (
        context->files.count == 0
        || (context->files.count == 1 && context->reference == NULL)
    ) {
        const char* option = NULL;
        ARG_WARN("missing file operand");
    }
})

static void chownPath(const char* path, uid_t uid, gid_t gid, Arguments* args);

static void chownDirectory(const char* path, uid_t uid, gid_t gid, Arguments* args) {
    DIR* dir = opendir(path);
    if (dir == NULL) {
        args->error = true;
        fprintf(stderr, "%s: cannot open '%s': %s\n", args->prog, path, strerror(errno));
        return;
    }
    struct dirent* entr = readdir(dir);
    List children;
    initList(&children);
    while (entr != NULL) {
        if (strcmp(entr->d_name, ".") != 0 && strcmp(entr->d_name, "..") != 0) {
            char* entr_path = joinPaths(path, entr->d_name);
            copyStringToList(&children, entr_path);
        }
        entr = readdir(dir);
    }
    closedir(dir);
    for (size_t i = 0; i < children.count; i++) {
        char* entr_path = LIST_GET(char*, children, i);
        chownPath(entr_path, uid, gid, args);
        free(entr_path);
    }
    deinitList(&children);
}

static void chownPath(const char* path, uid_t uid, gid_t gid, Arguments* args) {
    struct stat stats;
    if (stat(path, &stats) != 0) {
        args->error = true;
        fprintf(stderr, "%s: cannot stat '%s': %s\n", args->prog, path, strerror(errno));
        return;
    }
    if (chown(path, uid, gid) != 0) {
        args->error = true;
        fprintf(stderr, "%s: cannot chown '%s': %s\n", args->prog, path, strerror(errno));
        return;
    }
    if (args->recursive && (stats.st_mode & S_IFMT) == S_IFDIR) {
        chownDirectory(path, uid, gid, args);
    }
}

static void parseMode(const char* mode, uid_t* uid, gid_t* gid, Arguments* args) {
    if (*mode != ':') {
        uid_t value = 0;
        while (*mode != 0 && *mode != ':') {
            if (*mode >= '0' && *mode <= '9') {
                value = 10 * value + *mode - '0';
            } else {
                fprintf(stderr, "%s: invalid uid '%s'\n", args->prog, mode);
                exit(1);
            }
            mode++;
        }
        *uid = value;
    }
    if (*mode == ':') {
        mode++;
        if (*mode != 0) {
            gid_t value = 0;
            while (*mode != 0) {
                if (*mode >= '0' && *mode <= '9') {
                    value = 10 * value + *mode - '0';
                } else {
                    fprintf(stderr, "%s: invalid gid '%s'\n", args->prog, mode);
                    exit(1);
                }
                mode++;
            }
            *gid = value;
        }
    }
    if (*mode != 0) {
        fprintf(stderr, "%s: invalid owner '%s'\n", args->prog, mode);
        exit(1);
    }
}

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    args.recursive = false;
    args.error = false;
    args.reference = NULL;
    initList(&args.files);
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    uid_t uid = -1;
    gid_t gid = -1;
    if (args.reference == NULL) {
        parseMode(args.files.values[0], &uid, &gid, &args);
    } else {
        struct stat stats;
        if (stat(args.reference, &stats) != 0) {
            fprintf(stderr, "%s: cannot stat '%s': %s\n", args.prog, args.reference, strerror(errno));
            exit(1);
        }
        uid = stats.st_uid;
        gid = stats.st_gid;
    }
    for (size_t i = args.reference == NULL ? 1 : 0; i < args.files.count; i++) {
        char* path = LIST_GET(char*, args.files, i);
        chownPath(path, uid, gid, &args);
        free(path);
    }
    deinitList(&args.files);
    free(args.reference);
    return args.error ? 1 : 0;
}

