
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
    bool error;
    List files;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "cat <file>...", {
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
        copyStringToList(&context->files, "-");
    }
})

static void catFile(const char* path, Arguments* args) {
    FILE* file;
    if (strcmp(path, "-") == 0) {
        file = stdin;
    } else {
        file = fopen(path, "r");
        if (file == NULL) {
            args->error = true;
            fprintf(stderr, "%s: cannot open '%s': %s\n", args->prog, path, strerror(errno));
            return;
        }
    }
    char buffer[1024];
    size_t tmp_size;
    do {
        tmp_size = fread(buffer, 1, 1024, file);
        size_t left = tmp_size;
        while (left > 0) {
            left -= fwrite(buffer + tmp_size - left, 1, left, stdout);
        }
    } while (tmp_size != 0 && !feof(file));
    if (file != stdin) {
        fclose(file);
    }
}

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    args.error = false;
    initList(&args.files);
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    for (size_t i = 0; i < args.files.count; i++) {
        char* path = LIST_GET(char*, args.files, i);
        catFile(path, &args);
    }
    deinitListAndContents(&args.files);
    return args.error ? 1 : 0;
}

