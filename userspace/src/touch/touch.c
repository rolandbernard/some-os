
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
    bool access;
    bool nocreate;
    bool error;
    List files;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "touch [options] <file>...", {
    // Options
    ARG_FLAG('a', NULL, {
        context->access = true;
    }, "change only the access time");
    ARG_FLAG('c', "no-create", {
        context->nocreate = true;
    }, "do not create any files");
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

static void touchFile(const char* path, Arguments* args) {
    // We have no way of doing this correctly. (Would have to add a syscall)
    int fd = open(
        path, (args->nocreate ? 0 : O_CREAT) | (args->access ? O_RDONLY : O_APPEND | O_WRONLY), 0666
    );
    if (fd < 0) {
        args->error = true;
        fprintf(stderr, "%s: cannot open '%s': %s\n", args->prog, path, strerror(errno));
        return;
    }
    if (!args->access) {
        write(fd, NULL, 0);
    }
    close(fd);
}

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    args.access = false;
    args.nocreate = false;
    args.error = false;
    initList(&args.files);
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    for (size_t i = 0; i < args.files.count; i++) {
        char* path = LIST_GET(char*, args.files, i);
        touchFile(path, &args);
        free(path);
    }
    deinitList(&args.files);
    return args.error ? 1 : 0;
}

