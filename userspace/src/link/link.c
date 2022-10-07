
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "args.h"
#include "list.h"
#include "util.h"

typedef struct {
    const char* prog;
    const char* file1;
    const char* file2;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "link <file1> <file2>", {
    // Options
    ARG_FLAG(0, "help", {
        ARG_PRINT_HELP(argumentSpec, NULL);
        exit(0);
    }, "display this help and exit");
}, {
    // Default
    if (context->file1 == NULL) {
        context->file1 = value;
    } else if (context->file2 == NULL) {
        context->file2 = value;
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
    if (context->file2 == NULL) {
        const char* option = NULL;
        ARG_WARN("missing file operand");
    }
})

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    args.file1 = NULL;
    args.file2 = NULL;
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    if (link(args.file1, args.file2) != 0) {
        fprintf(stderr, "%s: cannot link '%s' at '%s': %s\n", args.prog, args.file1, args.file2, strerror(errno));
        return 1;
    } else {
        return 0;
    }
}

