
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
    struct tm time;
    char* format;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "date [options] [+<format>]", {
    // Options
    ARG_VALUED('r', "reference", {
        struct stat stats;
        if (stat(value, &stats) != 0) {
            fprintf(stderr, "%s: cannot stat '%s': %s\n", context->prog, value, strerror(errno));
            exit(1);
        }
        struct tm* tm = localtime(&stats.st_mtime);
        memcpy(&context->time, tm, sizeof(struct tm));
    }, false, "=<file>", "display the last modification time of the give file");
    ARG_FLAG(0, "help", {
        ARG_PRINT_HELP(argumentSpec, NULL);
        exit(0);
    }, "display this help and exit");
}, {
    // Default
    if (value[0] == '+' && context->format == NULL) {
        context->format = strdup(value + 1);
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
    if (context->format == NULL) {
        context->format = strdup("%c");
    }
})

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    time_t now = time(NULL);
    struct tm* tm_now = localtime(&now);
    memcpy(&args.time, tm_now, sizeof(struct tm));
    args.format = NULL;
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    char time[1024];
    strftime(time, 1024, args.format, &args.time);
    printf("%s\n", time);
    free(args.format);
    return 0;
}

