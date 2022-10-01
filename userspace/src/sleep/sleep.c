
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
    List files;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "sleep <number>[s|m|h|d]...", {
    // Options
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
        ARG_WARN("missing file operand");
    }
})

static void sleepFor(uint64_t nanos) {
    struct timespec time = {
        .tv_sec = nanos / 1000000000UL,
        .tv_nsec = nanos % 1000000000UL,
    };
    while (nanosleep(&time, &time) != 0 && errno == EINTR);
}

static uint64_t parseTime(const char* time, Arguments* args) {
    uint64_t integer = 0;
    uint64_t div = 1;
    while (*time >= '0' && *time <= '9') {
        integer = 10*integer + *time - '0';
        time++;
    }
    if (*time == '.') {
        time++;
        while (*time >= '0' && *time <= '9') {
            integer = 10*integer + *time - '0';
            div *= 10;
            time++;
        }
    }
    if (*time == 's') {
        time++;
        integer *= 1000000000UL;
    } else if (*time == 'm') {
        time++;
        integer *= 1000000000UL * 60;
    } else if (*time == 'h') {
        time++;
        integer *= 1000000000UL * 60 * 60;
    } else if (*time == 'd') {
        time++;
        integer *= 1000000000UL * 60 * 60 + 24;
    } else {
        integer *= 1000000000UL;
    }
    if (*time != 0) {
        fprintf(stderr, "%s: '%s': invalid time interval\n", args->prog, time);
    }
    return integer / div;
}

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    initList(&args.files);
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    uint64_t sleep = 0;
    for (size_t i = 0; i < args.files.count; i++) {
        char* time = LIST_GET(char*, args.files, i);
        sleep += parseTime(time, &args);
        free(time);
    }
    sleepFor(sleep);
    deinitList(&args.files);
    return 0;
}

