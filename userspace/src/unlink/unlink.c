
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
    const char* file;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "unlink <file>", {
    // Options
    ARG_FLAG(0, "help", {
        ARG_PRINT_HELP(argumentSpec, NULL);
        exit(0);
    }, "display this help and exit");
}, {
    // Default
    if (context->file == NULL) {
        context->file = value;
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
    if (context->file == NULL) {
        const char* option = NULL;
        ARG_WARN("missing file operand");
    }
})

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    args.file = NULL;
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    if (unlink(args.file) != 0) {
        fprintf(stderr, "%s: cannot unlink '%s': %s\n", args.prog, args.file, strerror(errno));
        return 1;
    } else {
        return 0;
    }
}

