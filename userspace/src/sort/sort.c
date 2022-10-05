
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#include "args.h"
#include "list.h"
#include "util.h"

typedef struct {
    const char* prog;
    bool ignore_blanks;
    bool dictionary;
    bool ignore_case;
    bool general_numeric;
    bool ignore_nonprint;
    bool human_numeric;
    bool numeric;
    bool random;
    bool reverse;
    bool zero;
    bool error;
    bool unique;
    char* output;
    char* key;
    List files;
    List lines;
} Arguments;

static char* readLineFromFile(FILE* file) {
    size_t length = 0;
    char* buffer = NULL;
    while (!feof(file)) {
        buffer = realloc(buffer, length + 1024);
        fgets(buffer + length, 1024, file);
        if (buffer[strlen(buffer) - 1] == '\n') {
            buffer[strlen(buffer) - 1] = 0;
            break;
        }
    }
    if (feof(file) && length == 0) {
        free(buffer);
        return NULL;
    } else {
        return buffer;
    }
}

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "sort [options] [file]...", {
    // Options
    ARG_FLAG('b', "ignore-leading-blanks", {
        context->ignore_blanks = true;
    }, "ignore leading blanks");
    ARG_FLAG('d', "dictionary-order", {
        context->dictionary = true;
    }, "consider only blanks and alphanumeric characters");
    ARG_FLAG('f', "ignore-case", {
        context->ignore_case = true;
    }, "fold lower case to upper case characters");
    ARG_FLAG('g', "general-numeric-sort", {
        context->general_numeric = true;
    }, "compare according to general numerical value");
    ARG_FLAG('i', "ignore-nonprinting", {
        context->ignore_nonprint = true;
    }, "consider only printable characters");
    ARG_FLAG('h', "human-numeric-sort", {
        context->human_numeric = true;
    }, "compare human readable numbers (e.g., 2K 1G)");
    ARG_FLAG('n', "numeric-sort", {
        context->numeric = true;
    }, "compare according to string numerical value");
    ARG_FLAG('R', "random-sort", {
        context->random = true;
    }, "shuffle, but group identical keys");
    ARG_FLAG('r', "reverse", {
        context->reverse = true;
    }, "reverse the result of comparisons");
    ARG_FLAG('u', "unique", {
        context->unique = true;
    }, "output only the first of an equal run");
    ARG_VALUED('o', "output", {
        context->output = strdup(value);
    }, false, "=<file>", "write results to the given file");
    ARG_FLAG('z', "zero-terminated", {
        context->zero = true;
    }, "line delimiter is NUL, not newline");
    ARG_VALUED('k', "key", {
        context->key = strdup(value);
    }, false, "=<key>", "sort via a key");
    ARG_FLAG(0, "help", {
        ARG_PRINT_HELP(argumentSpec, NULL);
        exit(0);
    }, "display this help and exit");
}, {
    // Default
    copyStringToList(&context->files, value);
}, {
    // Warning
    fprintf(stderr, "%s: '%s': %s\n", argv[0], option, warning);
    exit(2);
}, {
    // Final
    if (context->files.count == 0) {
        copyStringToList(&context->files, "-");
    }
})

static void readFile(const char* path, Arguments* args) {
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
    while (!feof(file)) {
        char* line = readLineFromFile(file);
        if (line != NULL) {
            addToList(&args->lines, line);
        }
    }
    if (file != stdin) {
        fclose(file);
    }
}

static int randoms[256];

static long long readHuman(const char* str) {
    long long val = 0;
    while (*str >= '0' && *str <= '9') {
        val *= 10;
        val += *str - '0';
    }
    if (str[0] == 'b') {
        val *= 512;
    } else if (str[0] == 'k' && str[1] == 'B') {
        val *= 1000;
    } else if (str[0] == 'K') {
        val *= 1024;
    } else if (str[0] == 'M') {
        if (str[1] == 'B') {
            val *= 1000UL * 1000;
        } else {
            val *= 1024UL * 1024;
        }
    } else if (str[0] == 'G') {
        if (str[1] == 'B') {
            val *= 1000UL * 1000 * 1000;
        } else {
            val *= 1024UL * 1024 * 1024;
        }
    } else if (str[0] == 'T') {
        if (str[1] == 'B') {
            val *= 1000UL * 1000 * 1000 * 1000;
        } else {
            val *= 1024UL * 1024 * 1024 * 1024;
        }
    } else if (str[0] == 'P') {
        if (str[1] == 'B') {
            val *= 1000UL * 1000 * 1000 * 1000 * 1000;
        } else {
            val *= 1024UL * 1024 * 1024 * 1024 * 1024;
        }
    } else if (str[0] == 'E') {
        if (str[1] == 'B') {
            val *= 1000UL * 1000 * 1000 * 1000 * 1000 * 1000;
        } else {
            val *= 1024UL * 1024 * 1024 * 1024 * 1024 * 1024;
        }
    } else if (str[0] == 'Z') {
        if (str[1] == 'B') {
            val *= 1000UL * 1000 * 1000 * 1000 * 1000 * 1000 * 1000;
        } else {
            val *= 1024UL * 1024 * 1024 * 1024 * 1024 * 1024 * 1024;
        }
    } else if (str[0] == 'Y') {
        if (str[1] == 'B') {
            val *= 1000UL * 1000 * 1000 * 1000 * 1000 * 1000 * 1000;
        } else {
            val *= 1024UL * 1024 * 1024 * 1024 * 1024 * 1024 * 1024;
        }
    }
    return val;
}

static long long hashString(const char* str) {
    long long val = 0;
    while (*str != 0) {
        val ^= randoms[(int)*str];
        str++;
    }
    return val;
}

static int compareStrings(const char* a, const char* b, Arguments* args, bool unique) {
    if (args->reverse) {
        const char* tmp = a;
        a = b;
        b = tmp;
    }
    if (args->random) {
        long long ra = hashString(a);
        long long rb = hashString(b);
        if (ra < rb) {
            return -1;
        } else if (ra > rb) {
            return 1;
        }
    }
    if (args->ignore_blanks) {
        while (isblank((int)*a)) {
            a++;
        }
        while (isblank((int)*b)) {
            b++;
        }
    }
    if (args->human_numeric) {
        long long lla = readHuman(a);
        long long llb = readHuman(b);
        if (lla < llb) {
            return -1;
        } else if (lla > llb) {
            return 1;
        } else if (unique) {
            return 0;
        }
    }
    if (args->general_numeric) {
        double fa = atof(a);
        double fb = atof(b);
        if (fa < fb) {
            return -1;
        } else if (fa > fb) {
            return 1;
        } else if (unique) {
            return 0;
        }
    }
    if (args->numeric) {
        long long lla = atoll(a);
        long long llb = atoll(b);
        if (lla < llb) {
            return -1;
        } else if (lla > llb) {
            return 1;
        } else if (unique) {
            return 0;
        }
    }
    while (*a != 0 || *b != 0) {
        if (args->dictionary) {
            while (*a != 0 && !isblank((int)*a) && !isalnum((int)*a)) {
                a++;
            }
            while (*b != 0 && !isblank((int)*b) && !isalnum((int)*b)) {
                b++;
            }
        }
        if (args->ignore_nonprint) {
            while (*a != 0 && !isprint((int)*a)) {
                a++;
            }
            while (*b != 0 && !isprint((int)*b)) {
                b++;
            }
        }
        char ca = *a;
        if (args->ignore_case && ca >= 'a' && ca <= 'z') {
            ca += 'A' - 'a';
        }
        char cb = *b;
        if (args->ignore_case && cb >= 'a' && cb <= 'z') {
            cb += 'A' - 'a';
        }
        if (ca < cb) {
            return -1;
        } else if (ca > cb) {
            return 1;
        }
        a++;
        b++;
    }
    return 0;
}

static Arguments args;

static char* readKeyFrom(const char** ptr) {
    char* ret = NULL;
    if (*ptr != NULL) {
        const char* end = strchr(*ptr, ',');
        if (end == NULL) {
            ret = strdup(*ptr);
            *ptr = NULL;
        } else {
            ret = malloc(end - *ptr + 1);
            memcpy(ret, *ptr, end - *ptr);
            ret[end - *ptr] = 0;
            *ptr += strlen(ret) + 1;
        }
    }
    return ret;
}

static char* readLineField(const char* line, int field) {
    if (field == 0) {
        return strdup(line);
    } else {
        while (field > 1 && *line != 0) {
            line++;
            if (isblank((int)*line) && !isblank((int)*(line - 1))) {
                field--;
            }
        }
        char* ret = strdup(line);
        char* end = ret;
        while (*end != 0 && isblank((int)*end)) {
            end++;
        }
        while (*end != 0 && !isblank((int)*end)) {
            end++;
        }
        *end = 0;
        return ret;
    }
}

static int compareLines(const char* a, const char* b, bool unique) {
    if (args.key != NULL) {
        const char* keys = args.key;
        char* key = readKeyFrom(&keys);
        while (key != NULL) {
            Arguments local_args;
            local_args.ignore_blanks = false;
            local_args.dictionary = false;
            local_args.ignore_case = false;
            local_args.general_numeric = false;
            local_args.ignore_nonprint = false;
            local_args.human_numeric = false;
            local_args.numeric = false;
            local_args.random = false;
            local_args.reverse = false;
            const char* str = key;
            int field = 0;
            while (*str >= '0' && *str <= '9') {
                field *= 10;
                field += *str - '0';
                str++;
            }
            if (*str == 0) {
                local_args = args;
            }
            while (*str != 0) {
                if (*str == 'b') {
                    local_args.ignore_blanks = true;
                } else if (*str == 'd') {
                    local_args.dictionary = true;
                } else if (*str == 'f') {
                    local_args.ignore_case = true;
                } else if (*str == 'g') {
                    local_args.general_numeric = true;
                } else if (*str == 'i') {
                    local_args.ignore_nonprint = true;
                } else if (*str == 'h') {
                    local_args.human_numeric = true;
                } else if (*str == 'n') {
                    local_args.numeric = true;
                } else if (*str == 'R') {
                    local_args.random = true;
                } else if (*str == 'r') {
                    local_args.reverse = true;
                } else {
                    fprintf(stderr, "%s: invalid field specification '%s'\n", args.prog, key);
                    exit(1);
                }
                str++;
            }
            free(key);
            char* fa = readLineField(a, field);
            char* fb = readLineField(b, field);
            int cmp = compareStrings(fa, fb, &local_args, true);
            free(fa);
            free(fb);
            if (cmp != 0) {
                return cmp;
            }
            key = readKeyFrom(&keys);
        }
        return unique ? 0 : compareStrings(a, b, &args, unique);
    } else {
        return compareStrings(a, b, &args, unique);
    }
}

static int compareLinesSort(const void* a_void, const void* b_void) {
    const char* a = *(const char* const*)a_void;
    const char* b = *(const char* const*)b_void;
    return compareLines(a, b, false);
}

int main(int argc, const char* const* argv) {
    args.prog = argv[0];
    args.ignore_blanks = false;
    args.dictionary = false;
    args.ignore_case = false;
    args.general_numeric = false;
    args.ignore_nonprint = false;
    args.human_numeric = false;
    args.numeric = false;
    args.random = false;
    args.reverse = false;
    args.output = NULL;
    args.key = NULL;
    args.unique = false;
    args.zero = false;
    args.error = false;
    initList(&args.files);
    initList(&args.lines);
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    for (size_t i = 0; i < args.files.count; i++) {
        char* path = LIST_GET(char*, args.files, i);
        readFile(path, &args);
        free(path);
    }
    deinitList(&args.files);
    if (args.random) {
        struct timeval val;
        gettimeofday(&val, NULL);
        srand(val.tv_sec ^ val.tv_usec);
        for (size_t i = 0; i < 256; i++) {
            randoms[i] = rand();
        }
    }
    qsort(args.lines.values, args.lines.count, sizeof(const char*), compareLinesSort);
    free(args.key);
    FILE* out = stdout;
    if (args.output != NULL) {
        out = fopen(args.output, "w");
        if (out == NULL) {
            fprintf(stderr, "%s: cannot open '%s': %s\n", args.prog, args.output, strerror(errno));
            return 1;
        }
    }
    free(args.output);
    for (size_t i = 0; i < args.lines.count; i++) {
        char* line = LIST_GET(char*, args.lines, i);
        if (args.unique && i != 0) {
            char* last_line = LIST_GET(char*, args.lines, i - 1);
            if (compareLines(last_line, line, true) == 0) {
                continue;
            }
        }
        if (args.zero) {
            fprintf(out, "%s", line);
            fputc('\0', out);
        } else {
            fprintf(out, "%s\n", line);
        }
    }
    deinitListAndContents(&args.lines);
    if (args.output != NULL) {
        fclose(out);
    }
    return args.error ? 1 : 0;
}

