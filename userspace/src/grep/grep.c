
#include <dirent.h>
#include <errno.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "args.h"
#include "list.h"
#include "util.h"

typedef enum {
    LINES,
    LINES_INV,
    COUNT,
    COUNT_INV,
    FILES_WITH,
    FILES_WITHOUT,
    MATCHES,
} OutputKind;

typedef struct {
    const char* prog;
    bool extended;
    bool fixed;
    bool icase;
    bool inv;
    OutputKind kind;
    bool line_number;
    bool line_regex;
    size_t limit;
    bool recursive;
    bool error;
    List patterns;
    List files;
} Arguments;

static char* readLineFromStr(const char** ptr) {
    char* ret = NULL;
    const char* end = strchr(*ptr, '\n');
    if (end == NULL) {
        ret = strdup(*ptr);
        *ptr = NULL;
    } else {
        ret = malloc(end - *ptr + 1);
        memcpy(ret, *ptr, end - *ptr);
        ret[end - *ptr] = 0;
        *ptr += strlen(ret) + 1;
    }
    return ret;
}

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
    return buffer;
}

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "grep [options] <patterns> [file]...", {
    // Options
    ARG_FLAG('c', "count", {
        context->kind = COUNT;
    }, "count the matching lines");
    ARG_FLAG('E', "extended-regexp", {
        context->extended = true;
    }, "interpret patterns as fixed strings");
    ARG_FLAG('l', "files-with-matches", {
        context->kind = FILES_WITH;
    }, "print filenames of files that contain a match");
    ARG_FLAG('L', "files-without-match", {
        context->kind = FILES_WITHOUT;
    }, "print filenames of files that contain no match");
    ARG_VALUED('m', "max-count", {
        context->limit = 0;
        while (*value != 0) {
            context->limit *= 10;
            if (*value >= '0' && *value <= '9') {
                context->limit += *value - '0';
            } else {
                const char* option = value;
                ARG_WARN("invalid count argument");
            }
        }
    }, false, "=<num>", "stop after finding <num> matches");
    ARG_FLAG('o', "only-matching", {
        context->kind = MATCHES;
    }, "print only matched parts");
    ARG_FLAG('n', "line number", {
        context->line_number = true;
    }, "prefix each line with the line number");
    ARG_FLAG('r', "recursive", {
        context->recursive = true;
    }, "read directories recursively");
    ARG_VALUED('e', "regexp", {
        const char* val = value;
        while (val != NULL) {
            char* line = readLineFromStr(&val);
            if (line != NULL && strlen(line) != 0) {
                addToList(&context->patterns, line);
            } else {
                free(line);
            }
        }
    }, false, "=<pattern>", "use patterns as the patterns");
    ARG_FLAG('F', "fixed-strings", {
        context->fixed = true;
    }, "interpret patterns as fixed strings");
    ARG_VALUED('f', "file", {
        FILE* file = fopen(value, "r");
        if (file == NULL) {
            fprintf(stderr, "%s: cannot open '%s': %s\n", context->prog, value, strerror(errno));
            exit(2);
        }
        while (!feof(file)) {
            char* line = readLineFromFile(file);
            if (line != NULL && strlen(line) != 0) {
                addToList(&context->patterns, line);
            } else {
                free(line);
            }
        }
        fclose(file);
    }, false, "=<file>", "obtain patterns from the given file");
    ARG_FLAG('i', "ignore-case", {
        context->icase = true;
    }, "ignore case in patterns");
    ARG_FLAG('v', "invert-match", {
        context->inv = true;
    }, "select non-matching lines");
    ARG_FLAG('x', "line-regexp", {
        context->line_regex = true;
    }, "only match whole lines");
    ARG_FLAG(0, "help", {
        ARG_PRINT_HELP(argumentSpec, NULL);
        exit(0);
    }, "display this help and exit");
}, {
    // Default
    if (context->patterns.count == 0) {
        const char* val = value;
        while (val != NULL) {
            char* line = readLineFromStr(&val);
            if (line != NULL && strlen(line) != 0) {
                addToList(&context->patterns, line);
            } else {
                free(line);
            }
        }
    } else {
        copyStringToList(&context->files, value);
    }
}, {
    // Warning
    fprintf(stderr, "%s: '%s': %s\n", argv[0], option, warning);
    exit(2);
}, {
    // Final
    if (context->files.count == 0) {
        if (context->recursive) {
            copyStringToList(&context->files, ".");
        } else {
            copyStringToList(&context->files, "-");
        }
    }
    if (context->inv) {
        if (context->kind == LINES) {
            context->kind = LINES_INV;
        } else if (context->kind == COUNT) {
            context->kind = COUNT_INV;
        }
    }
})

static void grepPath(const char* path, regex_t* regex, Arguments* args);

static void grepFile(const char* path, regex_t* regex, Arguments* args) {
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
    size_t count = 0;
    size_t line_num = 0;
    while (!feof(file) && count < args->limit) {
        char* line = readLineFromFile(file);
        if (line != NULL) {
            line_num++;
            regmatch_t match;
            if (regexec(regex, line, 1, &match, 0) == 0) {
                switch (args->kind) {
                    case LINES:
                        if (args->line_number) {
                            printf("%lu:%s\n", line_num, line);
                        } else {
                            printf("%s\n", line);
                        }
                        // fall through
                    case COUNT:
                    case FILES_WITH:
                        count++;
                        break;
                    case MATCHES: {
                        const char* offset = line;
                        do {
                            count++;
                            fwrite(offset + match.rm_so, 1, match.rm_eo - match.rm_so, stdout);
                            fputc('\n', stdout);
                            offset += match.rm_eo;
                        } while (regexec(regex, offset, 1, &match, 0) == 0);
                        break;
                    }
                    case LINES_INV:
                    case COUNT_INV:
                    case FILES_WITHOUT:
                        break;
                }
            } else {
                switch (args->kind) {
                    case LINES_INV:
                        if (args->line_number) {
                            printf("%lu:%s\n", line_num, line);
                        } else {
                            printf("%s\n", line);
                        }
                        // fall through
                    case COUNT_INV:
                    case FILES_WITHOUT:
                        count++;
                        break;
                    case LINES:
                    case FILES_WITH:
                    case COUNT:
                    case MATCHES:
                        break;
                }
            }
        }
        free(line);
    }
    if (file != stdin) {
        fclose(file);
    }
    switch (args->kind) {
        case COUNT:
        case COUNT_INV:
            if (args->files.count > 1) {
                printf("%s:%lu\n", path, count);
            } else {
                printf("%lu\n", count);
            }
            break;
        case FILES_WITH:
        case FILES_WITHOUT:
            if (count > 0) {
                printf("%s\n", path);
            }
            break;
        case LINES:
        case LINES_INV:
        case MATCHES:
            break;
    }
}

static void grepDirectory(const char* path, regex_t* regex, Arguments* args) {
    if (args->recursive) {
        DIR* dir = opendir(path);
        if (dir == NULL) {
            args->error = true;
            fprintf(stderr, "%s: cannot open '%s': %s\n", args->prog, path, strerror(errno));
            return;
        }
        struct dirent* entr = readdir(dir);
        while (entr != NULL) {
            if (strcmp(entr->d_name, ".") != 0 && strcmp(entr->d_name, "..") != 0) {
                char* entr_path = joinPaths(path, entr->d_name);
                grepPath(entr_path, regex, args);
                free(entr_path);
            }
            entr = readdir(dir);
        }
        closedir(dir);
    } else {
        args->error = true;
        fprintf(stderr, "%s: target '%s': %s\n", args->prog, path, strerror(EISDIR));
        return;
    }
}

static void grepPath(const char* path, regex_t* regex, Arguments* args) {
    if (strcmp(path, "-") == 0) {
        grepFile(path, regex, args);
    } else {
        struct stat stats;
        if (stat(path, &stats) != 0) {
            args->error = true;
            fprintf(stderr, "%s: cannot stat '%s': %s\n", args->prog, path, strerror(errno));
            return;
        }
        if ((stats.st_mode & S_IFMT) == S_IFDIR) {
            grepDirectory(path, regex, args);
        } else {
            grepFile(path, regex, args);
        }
    }
}

static void buildRegex(regex_t* regex, Arguments* args) {
    size_t string_size = 0;
    for (size_t i = 0; i < args->patterns.count; i++) {
        char* pat = LIST_GET(char*, args->patterns, i);
        string_size += 2 + strlen(pat) + 2 + 2;
    }
    char* string = malloc(string_size + 10);
    string_size = 0;
    if (args->line_regex) {
        string[string_size++] = '^';
        if (!args->extended) {
            string[string_size++] = '\\';
        }
        string[string_size++] = '(';
    }
    for (size_t i = 0; i < args->patterns.count; i++) {
        char* pat = LIST_GET(char*, args->patterns, i);
        if (i != 0) {
            if (!args->extended) {
                string[string_size++] = '\\';
            }
            string[string_size++] = '|';
        }
        if (!args->extended) {
            string[string_size++] = '\\';
        }
        string[string_size++] = '(';
        memcpy(string + string_size, pat, strlen(pat));
        string_size += strlen(pat);
        if (!args->extended) {
            string[string_size++] = '\\';
        }
        string[string_size++] = ')';
    }
    if (args->line_regex) {
        if (!args->extended) {
            string[string_size++] = '\\';
        }
        string[string_size++] = ')';
        string[string_size++] = '$';
    }
    string[string_size] = 0;
    int flags = 0;
    if (args->extended) {
        flags |= REG_EXTENDED;
    }
    if (args->icase) {
        flags |= REG_ICASE;
    }
    int err = regcomp(regex, string, flags);
    if (err != 0) {
        char error[1024];
        regerror(err, regex, error, 1024);
        fprintf(stderr, "%s: regex error: %s\n", args->prog, error);
        exit(1);
    }
    free(string);
}

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    args.extended = false;
    args.fixed = false;
    args.icase = false;
    args.kind = LINES;
    args.line_number = false;
    args.limit = SIZE_MAX;
    args.recursive = false;
    args.inv = false;
    args.error = false;
    initList(&args.patterns);
    initList(&args.files);
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    regex_t regex;
    buildRegex(&regex, &args);
    deinitListAndContents(&args.patterns);
    for (size_t i = 0; i < args.files.count; i++) {
        char* path = LIST_GET(char*, args.files, i);
        grepPath(path, &regex, &args);
        free(path);
    }
    deinitList(&args.files);
    return args.error ? 1 : 0;
}

