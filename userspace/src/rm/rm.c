
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
    bool force;
    bool recursive;
    bool directory;
    bool verbose;
    bool error;
    List files;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "rm [options] <file>...", {
    // Options
    ARG_FLAG('f', "force", {
        context->force = true;
    }, "ignore nonexistent files and arguments");
    ARG_FLAG('R', NULL, {
        context->recursive = true;
    }, "");
    ARG_FLAG('r', "recursive", {
        context->recursive = true;
    }, "remove directories and their contents recursively");
    ARG_FLAG('d', "dir", {
        context->directory = true;
    }, "remove empty directories");
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
    if (context->files.count == 0) {
        const char* option = NULL;
        ARG_WARN("missing operand");
    }
})

static void removePath(const char* path, Arguments* args);

static void removeFile(const char* path, Arguments* args) {
    if (remove(path) != 0) {
        args->error = true;
        fprintf(stderr, "%s: cannot remove '%s': %s\n", args->prog, path, strerror(errno));
        return;
    }
    if (args->verbose) {
        printf("removed '%s'\n", path);
    }
}

static void removeDirectory(const char* path, Arguments* args) {
    if (args->recursive || args->directory) {
        DIR* dir = opendir(path);
        if (dir == NULL) {
            args->error = true;
            fprintf(stderr, "%s: cannot remove '%s': %s\n", args->prog, path, strerror(errno));
            return;
        }
        struct dirent* entr = readdir(dir);
        List children;
        initList(&children);
        while (entr != NULL) {
            if (strcmp(entr->d_name, ".") != 0 && strcmp(entr->d_name, "..") != 0) {
                if (!args->recursive) {
                    args->error = true;
                    fprintf(stderr, "%s: cannot remove '%s': %s\n", args->prog, path, strerror(ENOTEMPTY));
                    closedir(dir);
                    return;
                } else {
                    char* entr_path = joinPaths(path, entr->d_name);
                    copyStringToList(&children, entr_path);
                }
            }
            entr = readdir(dir);
        }
        closedir(dir);
        for (size_t i = 0; i < children.count; i++) {
            char* entr_path = LIST_GET(char*, children, i);
            removePath(entr_path, args);
            free(entr_path);
        }
        deinitList(&children);
        if (rmdir(path) != 0) {
            args->error = true;
            fprintf(stderr, "%s: cannot remove '%s': %s\n", args->prog, path, strerror(errno));
            return;
        }
        if (args->verbose) {
            printf("removed directory '%s'\n", path);
        }
    } else {
        args->error = true;
        fprintf(stderr, "%s: cannot remove '%s': %s\n", args->prog, path, strerror(EISDIR));
        return;
    }
}

static void removePath(const char* path, Arguments* args) {
    struct stat stats;
    if (stat(path, &stats) != 0) {
        if ((errno != ENOENT && errno != ENOTDIR) || !args->force) {
            args->error = true;
            fprintf(stderr, "%s: cannot remove '%s': %s\n", args->prog, path, strerror(errno));
        }
        return;
    }
    if ((stats.st_mode & S_IFMT) == S_IFDIR) {
        removeDirectory(path, args);
    } else {
        removeFile(path, args);
    }
}

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    args.force = false;
    args.recursive = false;
    args.directory = false;
    args.verbose = false;
    args.error = false;
    initList(&args.files);
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    for (size_t i = 0; i < args.files.count; i++) {
        char* path = LIST_GET(char*, args.files, i);
        removePath(path, &args);
        free(path);
    }
    deinitList(&args.files);
    return args.error ? 1 : 0;
}

