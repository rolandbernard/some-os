
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
    bool clear;
    bool null;
    bool error;
    List unset;
    List envs;
    List command;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "env [options] [-] [<name>=<value>]... [<command> [arg]...]", {
    // Options
    ARG_FLAG('i', "ignore-environment", {
        context->clear = true;
    }, "start with an empty environment");
    ARG_FLAG('0', "null", {
        context->null = true;
    }, "end each output line with NUL");
    ARG_VALUED('u', "unset", {
        copyStringToList(&context->unset, value);
    }, false, "=<name>", "remove variable from environment");
    ARG_VALUED('C', "chdir", {
        if (chdir(value) != 0) {
            fprintf(stderr, "%s: cannot chdir '%s': %s\n", context->prog, value, strerror(errno));
            exit(1);
        }
    }, false, "=<directory>", "change working directory");
    ARG_FLAG(0, "help", {
        ARG_PRINT_HELP(argumentSpec, NULL);
        exit(0);
    }, "display this help and exit");
}, {
    // Default
    only_default = true;
    if (context->envs.count == 0 && value[0] == '-' && value[1] == 0) {
        context->clear = true;
    } else if (context->command.count == 0 && strstr(value, "=") != NULL) {
        copyStringToList(&context->envs, value);
    } else {
        copyStringToList(&context->command, value);
    }
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
    args.clear = false;
    args.null = false;
    args.error = false;
    initList(&args.unset);
    initList(&args.envs);
    initList(&args.command);
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    if (args.clear) {
        environ[0] = NULL;
    }
    for (size_t i = 0; i < args.unset.count; i++) {
        unsetenv(LIST_GET(char*, args.unset, i));
    }
    deinitListAndContents(&args.unset);
    for (size_t i = 0; i < args.envs.count; i++) {
        putenv(LIST_GET(char*, args.envs, i));
    }
    deinitListAndContents(&args.envs);
    if (args.command.count == 0) {
        for(char **current = environ; *current; current++) {
            fputs(*current, stdout);
            fputc(args.null ? 0 : '\n', stdout);
        }
    } else {
        addToList(&args.command, NULL);
        execvp(args.command.values[0], (char**)args.command.values);
        args.error = true;
        fprintf(
            stderr, "%s: cannot exec '%s': %s\n", args.prog, LIST_GET(char*, args.command, 0),
            strerror(errno)
        );
    }
    deinitListAndContents(&args.command);
    return args.error ? 1 : 0;
}

