
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <time.h>
#include <unistd.h>

#include "args.h"
#include "list.h"
#include "util.h"

typedef struct {
    const char* prog;
    bool recursive;
    bool error;
    char* reference;
    List times;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*,
    "chmod [options] <mode>[,...] <file>...\n"
    "  or:  chmod [options] <octal-mode> <file>...\n"
    "  or:  chmod [options] --reference=<file> <file>..."
, {
    // Options
    ARG_VALUED(0, "reference", {
        context->reference = strdup(value);
    }, false, "=<file>", "use the given files mode");
    ARG_FLAG('R', "recursive", {
        context->recursive = true;
    }, "change files and directories recursively");
    ARG_FLAG(0, "help", {
        ARG_PRINT_HELP(argumentSpec, NULL);
        exit(0);
    }, "display this help and exit");
}, {
    // Default
    copyStringToList(&context->times, value);
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
    if (
        context->times.count == 0
        || (context->times.count == 1 && context->reference == NULL)
    ) {
        const char* option = NULL;
        ARG_WARN("missing file operand");
    }
})

static void chmodPath(const char* path, mode_t clear, mode_t set, Arguments* args);

static void chmodDirectory(const char* path, mode_t clear, mode_t set, Arguments* args) {
    DIR* dir = opendir(path);
    if (dir == NULL) {
        args->error = true;
        fprintf(stderr, "%s: cannot open '%s': %s\n", args->prog, path, strerror(errno));
        return;
    }
    struct dirent* entr = readdir(dir);
    List children;
    initList(&children);
    while (entr != NULL) {
        if (strcmp(entr->d_name, ".") != 0 && strcmp(entr->d_name, "..") != 0) {
            char* entr_path = joinPaths(path, entr->d_name);
            copyStringToList(&children, entr_path);
        }
        entr = readdir(dir);
    }
    closedir(dir);
    for (size_t i = 0; i < children.count; i++) {
        char* entr_path = LIST_GET(char*, children, i);
        chmodPath(entr_path, clear, set, args);
        free(entr_path);
    }
    deinitList(&children);
}

static void chmodPath(const char* path, mode_t clear, mode_t set, Arguments* args) {
    struct stat stats;
    if (stat(path, &stats) != 0) {
        args->error = true;
        fprintf(stderr, "%s: cannot stat '%s': %s\n", args->prog, path, strerror(errno));
        return;
    }
    if (chmod(path, (stats.st_mode & ~clear) | set) != 0) {
        args->error = true;
        fprintf(stderr, "%s: cannot chmod '%s': %s\n", args->prog, path, strerror(errno));
        return;
    }
    if (args->recursive && (stats.st_mode & S_IFMT) == S_IFDIR) {
        chmodDirectory(path, clear, set, args);
    }
}

static void parseMode(const char* mode, mode_t* clear, mode_t* set, Arguments* args) {
    if (mode[0] == 'u' || mode[0] == 'g' || mode[0] == 'o' || mode[0] == 'a') {
        size_t idx = 0;
        while (mode[idx] == 'u' || mode[idx] == 'g' || mode[idx] == 'o' || mode[idx] == 'a') {
            idx++;
        }
        size_t op_idx = idx;
        if (mode[idx] == '-' || mode[idx] == '+' || mode[idx] == '=') {
            idx++;
        } else {
            fprintf(stderr, "%s: invalid mode '%s'\n", args->prog, mode);
            exit(1);
        }
        mode_t value = 0;
        while (mode[idx] == 'r' || mode[idx] == 'w' || mode[idx] == 'x' || mode[idx] == 's' || mode[idx] == 't') {
            if (mode[idx] == 'r') {
                value |= 0b100;
            } else if (mode[idx] == 'w') {
                value |= 0b010;
            } else if (mode[idx] == 'x') {
                value |= 0b001;
            } else if (mode[idx] == 's') {
                value |= 0b1000;
            } else if (mode[idx] == 't') {
                value |= 0b10000;
            }
            idx++;
        }
        size_t idx2 = 0;
        while (mode[idx2] == 'u' || mode[idx2] == 'g' || mode[idx2] == 'o' || mode[idx2] == 'a') {
            if (mode[idx2] == 'u') {
                if (mode[op_idx] == '=') {
                    *clear |= (0b111 << 6) | S_ISUID;
                }
                if (mode[op_idx] == '-') {
                    *clear |= (value & 0b111) << 6;
                    if ((value & 0b1000) != 0) {
                        *clear |= S_ISUID;
                    }
                } else {
                    *set |= (value & 0b111) << 6;
                    if ((value & 0b1000) != 0) {
                        *set |= S_ISUID;
                    }
                }
            } else if (mode[idx2] == 'g') {
                if (mode[op_idx] == '=') {
                    *clear |= (0b111 << 3) | S_ISGID;
                }
                if (mode[op_idx] == '-') {
                    *clear |= (value & 0b111) << 3;
                    if ((value & 0b1000) != 0) {
                        *clear |= S_ISGID;
                    }
                } else {
                    *set |= (value & 0b111) << 3;
                    if ((value & 0b1000) != 0) {
                        *set |= S_ISGID;
                    }
                }
            } else if (mode[idx2] == 'o') {
                if (mode[op_idx] == '=') {
                    *clear |= 0b111 | S_ISVTX;
                }
                if (mode[op_idx] == '-') {
                    *clear |= value & 0b111;
                    if ((value & 0b10000) != 0) {
                        *clear |= S_ISVTX;
                    }
                } else {
                    *set |= value & 0b111;
                    if ((value & 0b10000) != 0) {
                        *set |= S_ISVTX;
                    }
                }
            } else if (mode[idx2] == 'a') {
                if (mode[op_idx] == '=') {
                    *clear |= 0b111111111 | S_ISUID | S_ISGID | S_ISVTX;
                }
                if (mode[op_idx] == '-') {
                    *clear |= ((value & 0b111) << 6) | ((value & 0b111) << 3) | (value & 0b111);
                    if ((value & 0b1000) != 0) {
                        *clear |= S_ISUID | S_ISGID;
                    }
                    if ((value & 0b10000) != 0) {
                        *clear |= S_ISVTX;
                    }
                } else {
                    *set |= ((value & 0b111) << 6) | ((value & 0b111) << 3) | (value & 0b111);
                    if ((value & 0b1000) != 0) {
                        *set |= S_ISUID | S_ISGID;
                    }
                    if ((value & 0b10000) != 0) {
                        *set |= S_ISVTX;
                    }
                }
            }
            idx2++;
        }
        if (mode[idx] == ',') {
            parseMode(mode + idx + 1, clear, set, args);
        } else if (mode[idx] != 0) {
            fprintf(stderr, "%s: invalid mode '%s'\n", args->prog, mode);
            exit(1);
        }
    } else {
        mode_t value = 0;
        size_t idx = 0;
        if (mode[0] == '+' || mode[0] == '-' || mode[0] == '=') {
            idx++;
        }
        while (mode[idx] != 0) {
            if (mode[idx] >= '0' && mode[idx] <= '9') {
                value = 8 * value + mode[idx] - '0';
            } else {
                fprintf(stderr, "%s: invalid mode '%s'\n", args->prog, mode);
                exit(1);
            }
            idx++;
        }
        if (mode[0] == '+') {
            *clear = 0;
            *set = value;
        } else if (mode[0] == '-') {
            *clear = value;
            *set = 0;
        } else {
            *clear = ~0;
            *set = value;
        }
    }
}

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    args.recursive = false;
    args.error = false;
    args.reference = NULL;
    initList(&args.times);
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    mode_t clear = 0;
    mode_t set = 0;
    if (args.reference == NULL) {
        parseMode(args.times.values[0], &clear, &set, &args);
    } else {
        struct stat stats;
        if (stat(args.reference, &stats) != 0) {
            fprintf(stderr, "%s: cannot stat '%s': %s\n", args.prog, args.reference, strerror(errno));
            exit(1);
        }
        clear = ~0;
        set = stats.st_mode;
    }
    for (size_t i = args.reference == NULL ? 1 : 0; i < args.times.count; i++) {
        char* path = LIST_GET(char*, args.times, i);
        chmodPath(path, clear, set, &args);
        free(path);
    }
    deinitList(&args.times);
    free(args.reference);
    return args.error ? 1 : 0;
}

