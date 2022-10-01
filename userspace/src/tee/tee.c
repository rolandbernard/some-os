
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
    bool append;
    bool error;
    List files;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "tee [options] <file>...", {
    // Options
    ARG_FLAG('a', "append", {
        context->append = true;
    }, "append to the given files instead of overwriting");
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
})

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    initList(&args.files);
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    FILE* files[args.files.count + 1];
    for (size_t i = 0; i < args.files.count; i++) {
        char* path = LIST_GET(char*, args.files, i);
        files[i] = fopen(path, args.append ? "a" : "w");
        if (files[i] == NULL) {
            args.error = true;
            fprintf(stderr, "%s: cannot open '%s': %s\n", args.prog, path, strerror(errno));
        }
    }
    files[args.files.count] = stdout;
    char buffer[1024];
    size_t tmp_size;
    do {
        tmp_size = fread(buffer, 1, 1024, stdin);
        for (size_t i = 0; i <= args.files.count; i++) {
            if (files[i] != NULL) {
                size_t left = tmp_size;
                while (left > 0) {
                    left -= fwrite(buffer, 1, tmp_size, files[i]);
                }
            }
        }
    } while (tmp_size != 0 && !feof(stdin));
    deinitListAndContents(&args.files);
    return args.error ? 1 : 0;
}

