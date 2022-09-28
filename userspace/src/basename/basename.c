
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
    char* suffix;
    bool multiple;
    bool zero;
    List paths;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "ls [options] [file]...", {
    // Options
    ARG_FLAG('a', "multiple", {
        context->multiple = true;
    }, "do not ignore entries starting with .");
    ARG_VALUED('s', "suffix", {
        free(context->suffix);
        context->suffix = strdup(value);
        context->multiple = true;
    }, false, "={none|size|time}", "select sorting mode (-U, -S, -t)");
    ARG_FLAG('z', "zero", {
        context->zero = true;
    }, "sort by time, newest first");
    ARG_FLAG(0, "help", {
        ARG_PRINT_HELP(argumentSpec, NULL);
        exit(0);
    }, "display this help and exit");
}, {
    // Default
    only_default = true;
    if (context->multiple || context->paths.count == 0) {
        copyStringToList(&context->paths, value);
    } else if (context->paths.count == 1) {
        free(context->suffix);
        context->suffix = strdup(value);
    } else {
        const char* option = value;
        ARG_WARN("extra operand");
    }
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
    args.suffix = NULL;
    args.multiple = false;
    args.zero = false;
    initList(&args.paths);
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    for (size_t i = 0; i < args.paths.count; i++) {
        char* path = LIST_GET(char*, args.paths, i);
        size_t path_len = strlen(path);
        size_t dir_len = path_len;
        while (dir_len != 0 && path[dir_len - 1] != '/') {
            dir_len--;
        }
        if (args.suffix != NULL) {
            size_t suffix_len = strlen(args.suffix);
            if (
                path_len - suffix_len != dir_len
                && strcmp(path + path_len - suffix_len, args.suffix) == 0
            ) {
                path[path_len - suffix_len] = 0;
            }
        }
        fputs(path + dir_len, stdout);
        fputc(args.zero ? 0 : '\n', stdout);
        free(path);
    }
    deinitList(&args.paths);
    return 0;
}

