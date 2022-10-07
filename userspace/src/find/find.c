
#include <dirent.h>
#include <errno.h>
#include <regex.h>
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
    bool error;
    List paths;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "find [options] [path]...", {
    // Options
    ARG_FLAG(0, "help", {
        ARG_PRINT_HELP(argumentSpec, NULL);
        exit(0);
    }, "display this help and exit");
}, {
    // Default
    copyStringToList(&context->paths, value);
}, {
    // Warning
    fprintf(stderr, "%s: '%s': %s\n", argv[0], option, warning);
    exit(2);
}, {
    // Final
    if (context->paths.count == 0) {
        copyStringToList(&context->paths, ".");
    }
})

void listPath(const char* path, Arguments* args) {
    struct stat stats;
    if (stat(path, &stats) != 0) {
        args->error = true;
        fprintf(stderr, "%s: cannot stat '%s': %s\n", args->prog, path, strerror(errno));
        return;
    }
    printf("%s\n", path);
    if ((stats.st_mode & S_IFMT) == S_IFDIR) {
        DIR* dir = opendir(path);
        if (dir == NULL) {
            args->error = true;
            fprintf(stderr, "%s: cannot open '%s': %s\n", args->prog, path, strerror(errno));
            return;
        }
        struct dirent* entr = readdir(dir);
        while (entr != NULL) {
            if (strcmp(entr->d_name, ".") != 0 && strcmp(entr->d_name, "..") != 0) {
                char* entr_path = joinPaths(path, entr->d_name);
                listPath(entr_path, args);
                free(entr_path);
            }
            entr = readdir(dir);
        }
        closedir(dir);
    }
}

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    args.error = false;
    initList(&args.paths);
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    for (size_t i = 0; i < args.paths.count; i++) {
        char* path = LIST_GET(char*, args.paths, i);
        listPath(path, &args);
        free(path);
    }
    deinitList(&args.paths);
    return args.error ? 1 : 0;
}

