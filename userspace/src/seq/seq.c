
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
    char* sep;
    const char* first;
    const char* increment;
    const char* last;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "unlink <file>", {
    // Options
    ARG_VALUED('s', "separator", {
        free(context->sep);
        context->sep = strdup(value);
    }, false, "=<string>", "use the given string to separate numbers");
    ARG_FLAG(0, "help", {
        ARG_PRINT_HELP(argumentSpec, NULL);
        exit(0);
    }, "display this help and exit");
}, {
    // Default
    if (context->last == NULL) {
        context->last = value;
    } else if (context->first == NULL) {
        context->first = context->last;
        context->last = value;
    } else if (context->increment == NULL) {
        context->increment = context->last;
        context->last = value;
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
    if (context->last == NULL) {
        const char* option = NULL;
        ARG_WARN("missing file operand");
    }
    if (context->first == NULL) {
        context->first = "1";
    }
    if (context->increment == NULL) {
        context->increment = "1";
    }
    if (context->sep == NULL) {
        context->sep = strdup("\n");
    }
})

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    args.sep = NULL;
    args.first = NULL;
    args.increment = NULL;
    args.last = NULL;
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    double first = atof(args.first);
    double increment = atof(args.increment);
    double last = atof(args.last);
    while (first <= last) {
        printf("%.15g%s", first, args.sep);
        first += increment;
    }
    free(args.sep);
    return 0;
}

