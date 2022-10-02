
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
    size_t skip1;
    size_t skip2;
    size_t limit;
    bool byte;
    bool silent;
    bool verbose;
    bool different;
    List ops;
} Arguments;

static const char* parseCount(const char* count, size_t* out) {
    size_t value = 0;
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
    *out = value;
    return count;
}

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "cmp [options] <file1> [<file2> [<skip1> [<skip2>]]]", {
    // Options
    ARG_FLAG('b', "print-bytes", {
        context->byte = true;
    }, "print differing bytes");
    ARG_VALUED('i', "ignore-initial", {
        const char* end = parseCount(value, &context->skip1);
        if (*end == 0) {
            context->skip2 = context->skip1;
        } else if (*end == ':') {
            end = parseCount(end + 1, &context->skip2);
        }
        if (*end != 0) {
            ARG_WARN("invalid skip count");
        }
    }, false, "={<skip>|<skip1>:<skip2>}", "skip first bytes of the inputs");
    ARG_VALUED('n', "bytes", {
        if (*parseCount(value, &context->limit) != 0) {
            ARG_WARN("invalid limit count");
        }
    }, false, "=<limit>", "compare at most this many bytes");
    ARG_FLAG(0, "quiet", {
        if (context->verbose) {
            ARG_WARN("conflicting options");
        }
        context->silent = false;
    }, "");
    ARG_FLAG('s', "silent", {
        if (context->verbose) {
            ARG_WARN("conflicting options");
        }
        context->silent = true;
    }, "never print headers giving file names");
    ARG_FLAG('l', "verbose", {
        if (context->silent) {
            ARG_WARN("conflicting options");
        }
        context->verbose = true;
    }, "print byte numbers and differing byte values");
    ARG_FLAG(0, "help", {
        ARG_PRINT_HELP(argumentSpec, NULL);
        exit(0);
    }, "display this help and exit");
}, {
    // Default
    copyStringToList(&context->ops, value);
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
    if (context->ops.count == 0) {
        const char* option = NULL;
        ARG_WARN("missing file operand");
    } else if (context->ops.count == 1) {
        copyStringToList(&context->ops, "-");
    } else if (context->ops.count == 3) {
        if (*parseCount(context->ops.values[2], &context->skip1) != 0) {
            const char* option = context->ops.values[2];
            ARG_WARN("invalid skip count");
        }
    } else if (context->ops.count == 4) {
        if (*parseCount(context->ops.values[3], &context->skip2) != 0) {
            const char* option = context->ops.values[3];
            ARG_WARN("invalid skip count");
        }
    } else if (context->ops.count > 4) {
        const char* option = context->ops.values[4];
        ARG_WARN("extra operand");
    }
})

static FILE* openFile(const char* path, Arguments* args) {
    FILE* file;
    if (strcmp(path, "-") == 0) {
        file = stdin;
    } else {
        file = fopen(path, "r");
        if (file == NULL) {
            fprintf(stderr, "%s: cannot open '%s': %s\n", args->prog, path, strerror(errno));
            exit(2);
        }
    }
    return file;
}

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    args.skip1 = 0;
    args.skip2 = 0;
    args.limit = SIZE_MAX;
    args.silent = false;
    args.verbose = false;
    args.different = false;
    initList(&args.ops);
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    const char* path1 = args.ops.values[0];
    const char* path2 = args.ops.values[1];
    FILE* file1 = openFile(path1, &args);
    fseek(file1, args.skip1, SEEK_SET);
    FILE* file2 = openFile(path2, &args);
    fseek(file2, args.skip2, SEEK_SET);
    for (size_t byte = 0; byte < args.limit; byte++) {
        int c1 = fgetc(file1);
        int c2 = fgetc(file2);
        if (c1 != c2) {
            args.different = true;
            if (c1 == EOF) {
                printf("%s: EOF on %s after byte %lu\n", args.prog, path1, byte);
                break;
            } else if (c2 == EOF) {
                printf("%s: EOF on %s after byte %lu\n", args.prog, path2, byte);
                break;
            } else {
                if (args.verbose) {
                    printf("%li %o %o\n", byte + 1, c1, c2);
                } else if (!args.silent) {
                    printf("%s %s differ: byte %li", path1, path2, byte + 1);
                    if (args.byte) {
                        printf(" is %o %o", c1, c2);
                    }
                    printf("\n");
                    break;
                }
            }
        }
    }
    deinitListAndContents(&args.ops);
    return args.different ? 1 : 0;
}

