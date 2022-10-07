
#include <stdio.h>
#include <stdlib.h>

#include "args.h"

typedef struct {
    const char* prog;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "clear", {
    // Options
    ARG_FLAG(0, "help", {
        ARG_PRINT_HELP(argumentSpec, NULL);
        exit(0);
    }, "display this help and exit");
}, {
    // Default
    const char* option = value;
    ARG_WARN("extra operand");
}, {
    // Warning
    if (option != NULL) {
        fprintf(stderr, "%s: '%s': %s\n", argv[0], option, warning);
    } else {
        fprintf(stderr, "%s: %s\n", argv[0], warning);
    }
    exit(2);
})

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    printf("\e[H\e[J");
    fflush(stdout);
    return 0;
}

