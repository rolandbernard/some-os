
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
    off_t count;
    bool silent;
    bool verbose;
    bool zero;
    bool error;
    List files;
} Arguments;

static bool parseCount(const char* count, off_t* out) {
    off_t value = 0;
    bool neg = *count == '-';
    if (neg) {
        count++;
    }
    while (*count >= '0' && *count <= '9') {
        value = 10 * value + *count - '0';
        count++;
    }
    if (count[0] == 'b') {
        count++;
        value *= 512;
    } else if (count[0] == 'k' && count[1] == 'B') {
        count += 2;
        value *= 1000;
    } else if (count[0] == 'K') {
        count++;
        value *= 1024;
    } else if (count[0] == 'M') {
        if (count[1] == 'B') {
            count += 2;
            value *= 1000UL * 1000;
        } else {
            count++;
            value *= 1024UL * 1024;
        }
    } else if (count[0] == 'G') {
        if (count[1] == 'B') {
            count += 2;
            value *= 1000UL * 1000 * 1000;
        } else {
            count++;
            value *= 1024UL * 1024 * 1024;
        }
    } else if (count[0] == 'T') {
        if (count[1] == 'B') {
            count += 2;
            value *= 1000UL * 1000 * 1000 * 1000;
        } else {
            count++;
            value *= 1024UL * 1024 * 1024 * 1024;
        }
    } else if (count[0] == 'P') {
        if (count[1] == 'B') {
            count += 2;
            value *= 1000UL * 1000 * 1000 * 1000 * 1000;
        } else {
            count++;
            value *= 1024UL * 1024 * 1024 * 1024 * 1024;
        }
    } else if (count[0] == 'E') {
        if (count[1] == 'B') {
            count += 2;
            value *= 1000UL * 1000 * 1000 * 1000 * 1000 * 1000;
        } else {
            count++;
            value *= 1024UL * 1024 * 1024 * 1024 * 1024 * 1024;
        }
    } else if (count[0] == 'Z') {
        if (count[1] == 'B') {
            count += 2;
            value *= 1000UL * 1000 * 1000 * 1000 * 1000 * 1000 * 1000;
        } else {
            count++;
            value *= 1024UL * 1024 * 1024 * 1024 * 1024 * 1024 * 1024;
        }
    } else if (count[0] == 'Y') {
        if (count[1] == 'B') {
            count += 2;
            value *= 1000UL * 1000 * 1000 * 1000 * 1000 * 1000 * 1000;
        } else {
            count++;
            value *= 1024UL * 1024 * 1024 * 1024 * 1024 * 1024 * 1024;
        }
    }
    *out = neg ? -value : value;
    return *count == 0;
}

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "head [options] <file>...", {
    // Options
    ARG_VALUED('c', "bytes", {
        context->bytes = true;
        if (!parseCount(value, &context->count)) {
            ARG_WARN("invalid byte count");
        }
    }, false, "=[-]<number>", "");
    ARG_VALUED('n', "lines", {
        context->bytes = false;
        if (!parseCount(value, &context->count)) {
            ARG_WARN("invalid line count");
        }
    }, false, "=[-]<number>", "");
    ARG_FLAG('q', "quiet", {
        context->silent = true;
        context->verbose = false;
    }, "");
    ARG_FLAG(0, "silent", {
        context->silent = true;
        context->verbose = false;
    }, "never print headers giving file names");
    ARG_FLAG('v', "verbose", {
        context->silent = false;
        context->verbose = true;
    }, "always print headers giving file names");
    ARG_FLAG('z', "zero-terminated", {
        context->zero = true;
    }, "line delimiter is NULL, not newline");
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
    if (!args->silent && (args->files.count > 1 || args->verbose)) {
        printf("%s:\n", path);
    }
    if (args->count >= 0) {
        char buffer[1024];
        size_t tmp_size;
        size_t left = args->count;
        while (left > 0) {
            tmp_size = fread(buffer, 1, 1024, file);
            if (args->bytes) {
                if (tmp_size > left) {
                    tmp_size = left;
                }
                left -= tmp_size;
            } else {
                for (size_t i = 0; i < tmp_size; i++) {
                    if (buffer[i] == (args->zero ? 0 : '\n')) {
                        left--;
                        if (left == 0) {
                            tmp_size = i + 1;
                        }
                    }
                }
            }
            size_t tmp_left = tmp_size;
            while (tmp_left > 0) {
                tmp_left -= fwrite(buffer, 1, tmp_size, stdout);
            }
            if (tmp_size == 0 || feof(file)) {
                break;
            }
        }
        if (file != stdin) {
            fclose(file);
        }
    } else {
        off_t total = 0;
        size_t buffer_size = 0;
        char* buffer = NULL;
        size_t tmp_size;
        do {
            buffer = realloc(buffer, buffer_size + 1024);
            tmp_size = fread(buffer + buffer_size, 1, 1024, file);
            if (args->bytes) {
                total += tmp_size;
            } else {
                for (size_t i = 0; i < tmp_size; i++) {
                    if (buffer[buffer_size + i] == (args->zero ? 0 : '\n')) {
                        total++;
                    }
                }
            }
            buffer_size += tmp_size;
        } while (tmp_size != 0 && !feof(file));
        size_t left = args->count + total;
        char* next_out = buffer;
        while (left > 0) {
            if (args->bytes) {
                left--;
            } else {
                if (*next_out == (args->zero ? 0 : '\n')) {
                    left--;
                }
            }
            fputc(*next_out, stdout);
            next_out++;
        }
        free(buffer);
    }
}

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    args.bytes = false;
    args.count = 10;
    args.silent = false;
    args.verbose = false;
    args.zero = false;
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

