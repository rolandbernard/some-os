
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
    bool zero;
    List paths;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "dirname [options] [path]...", {
    // Options
    ARG_FLAG('z', "zero", {
        context->zero = true;
    }, "end each output with NUL, not newline");
    ARG_FLAG(0, "help", {
        ARG_PRINT_HELP(argumentSpec, NULL);
        exit(0);
    }, "display this help and exit");
}, {
    // Default
    copyStringToList(&context->paths, value);
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
    if (context->paths.count == 0) {
        const char* option = NULL;
        ARG_WARN("missing operand");
    }
})

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    args.zero = false;
    initList(&args.paths);
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    for (size_t i = 0; i < args.paths.count; i++) {
        char* path = LIST_GET(char*, args.paths, i);
        size_t path_len = strlen(path);
        size_t dir_len = path_len;
        while (dir_len > 0 && path[dir_len - 1] == '/') {
            dir_len--;
        }
        while (dir_len > 0 && path[dir_len - 1] != '/') {
            dir_len--;
        }
        while (dir_len > 1 && path[dir_len - 1] == '/') {
            dir_len--;
        }
        if (dir_len == 0) {
            path[0] = '.';
            path[1] = 0;
        } else {
            path[dir_len] = 0;
        }
        fputs(path, stdout);
        fputc(args.zero ? 0 : '\n', stdout);
        free(path);
    }
    deinitList(&args.paths);
    return 0;
}

