
#include <ctype.h>
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
    bool bytes;
    bool lines;
    bool words;
    bool error;
    List files;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "wc [options] <file>...", {
    // Options
    ARG_FLAG('c', "bytes", {
        context->bytes = true;
    }, "print the byte counts");
    ARG_FLAG('l', "lines", {
        context->lines = true;
    }, "print the newline counts");
    ARG_FLAG('w', "words", {
        context->words = true;
    }, "print the word counts");
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
    if (!context->bytes && !context->words && !context->lines) {
        context->bytes = true;
        context->words = true;
        context->lines = true;
    }
})

static void catFile(const char* path, size_t* total_lines, size_t* total_words, size_t* total_bytes, Arguments* args) {
    struct stat stats;
    if (stat(path, &stats) != 0) {
        args->error = true;
        fprintf(stderr, "%s: cannot stat '%s': %s\n", args->prog, path, strerror(errno));
        return;
    }
    if ((stats.st_mode & S_IFMT) == S_IFDIR) {
        args->error = true;
        fprintf(stderr, "%s: cannot stat '%s': %s\n", args->prog, path, strerror(EISDIR));
        return;
    }
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
    size_t lines = 0;
    size_t words = 0;
    size_t bytes = 0;
    char buffer[1024];
    bool last_space = true;
    size_t tmp_size;
    do {
        tmp_size = fread(buffer, 1, 1024, file);
        for (size_t i = 0; i < tmp_size; i++) {
            if (buffer[i] == '\n') {
                lines++;
            }
            if (isspace((int)buffer[i])) {
                if (!last_space) {
                    words++;
                }
                last_space = true;
            } else {
                last_space = false;
            }
        }
        bytes += tmp_size;
    } while (tmp_size != 0 && !feof(file));
    if (!last_space) {
        words++;
    }
    if (args->lines) {
        printf("%lu ", lines);
    }
    if (args->words) {
        printf("%lu ", words);
    }
    if (args->bytes) {
        printf("%lu ", bytes);
    }
    printf("%s\n", path);
    *total_lines += lines;
    *total_words += words;
    *total_bytes += bytes;
    if (file != stdin) {
        fclose(file);
    }
}

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    args.bytes = false;
    args.lines = false;
    args.words = false;
    args.error = false;
    initList(&args.files);
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    size_t total_lines = 0;
    size_t total_words = 0;
    size_t total_bytes = 0;
    for (size_t i = 0; i < args.files.count; i++) {
        char* path = LIST_GET(char*, args.files, i);
        catFile(path, &total_lines, &total_words, &total_bytes, &args);
    }
    if (args.files.count > 1) {
        if (args.lines) {
            printf("%lu ", total_lines);
        }
        if (args.words) {
            printf("%lu ", total_words);
        }
        if (args.bytes) {
            printf("%lu ", total_bytes);
        }
        printf("total\n");
    }
    deinitListAndContents(&args.files);
    return args.error ? 1 : 0;
}

