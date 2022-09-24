
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "args.h"
#include "list.h"
#include "util.h"

typedef enum {
    SORT_NONE,
    SORT_NAME,
    SORT_SIZE,
    SORT_TIME,
} Sorting;

typedef enum {
    FILTER_DEFAULT,
    FILTER_ALMOST_ALL,
    FILTER_ALL,
} Filter;

typedef enum {
    FORMAT_NONE,
    FORMAT_HUMAN,
    FORMAT_SI,
} SizeFormat;

typedef struct {
    const char* prog;
    Filter filter;
    Sorting sort;
    SizeFormat format;
    bool recursive;
    bool directory;
    bool long_fmt;
    bool reverse;
    List files;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "ls [options] [file]...", {
    // Options
    ARG_FLAG('a', "all", {
        context->filter = FILTER_ALL;
    }, "do not ignore entries starting with .");
    ARG_FLAG('A', "--almost-all", {
        context->filter = FILTER_ALMOST_ALL;
    }, "do not list . and ..");
    ARG_FLAG('d', "directory", {
        context->directory = true;
    }, "list directories themselves, not their contents");
    ARG_FLAG('f', NULL, {
        context->filter = FILTER_ALL;
        context->sort = SORT_NONE;
    }, "list all entries in directory order");
    ARG_FLAG('h', "human-readable", {
        context->format = FORMAT_HUMAN;
    }, "with -l, print sizes like 1K 234M 2G etc.");
    ARG_FLAG(0, "si", {
        context->format = FORMAT_SI;
    }, "likewise, but use powers of 1000 not 1024");
    ARG_FLAG('l', NULL, {
        context->long_fmt = true;
    }, "use long listing format");
    ARG_FLAG('r', "reverse", {
        context->reverse = true;
    }, "reverse order while sorting");
    ARG_FLAG('R', "recursive", {
        context->recursive = true;
    }, "list subdirectories recursively");
    ARG_FLAG('S', NULL, {
        context->sort = SORT_SIZE;
    }, "sort by file size, largest first");
    ARG_VALUED(0, "sort", {
        if (strcmp(value, "none") == 0) {
            context->sort = SORT_NONE;
        } else if (strcmp(value, "size") == 0) {
            context->sort = SORT_SIZE;
        } else if (strcmp(value, "time") == 0) {
            context->sort = SORT_TIME;
        } else {
            ARG_WARN_UNKNOWN_VALUE();
        }
    }, false, "={none|size|time}", "select sorting mode (-U, -S, -t)");
    ARG_FLAG('t', NULL, {
        context->sort = SORT_TIME;
    }, "sort by time, newest first");
    ARG_FLAG('U', NULL, {
        context->sort = SORT_NONE;
    }, "do not sort; list entries in directory order");
    ARG_FLAG(0, "help", {
        ARG_PRINT_HELP(argumentSpec, NULL);
        exit(0);
    }, "display this help and exit");
}, {
    // Default
    copyStringToList(&context->files, value);
}, {
    // Warning
    fprintf(stderr, "%s: %s: %s\n", argv[0], option, warning);
    exit(2);
}, {
    // Final
    if (context->files.count == 0) {
        copyStringToList(&context->files, ".");
    }
    if (context->reverse && context->filter == FILTER_ALL) {
        context->filter = FILTER_ALMOST_ALL;
    }
})

typedef struct {
    mode_t mode;
    nlink_t nlink;
    off_t size;
    time_t time;
    char* name;
} Entry;

Entry* entryForPath(const char* path, Arguments* args) {
    Entry* result = (Entry*)calloc(1, sizeof(Entry));
    struct stat stats;
    if (stat(path, &stats) == 0) {
        result->mode = stats.st_mode;
        result->nlink = stats.st_nlink;
        result->size = stats.st_size;
        result->time = stats.st_mtime;
    }
    result->name = strdup(basename(path));
    return result;
}

int compareName(const void* a, const void* b) {
    return strcasecmp((*(Entry* const*)a)->name, (*(Entry* const*)b)->name);
}

int compareSize(const void* a, const void* b) {
    return (*(Entry* const*)b)->size - (*(Entry* const*)a)->size;
}

int compareTime(const void* a, const void* b) {
    return (*(Entry* const*)b)->time - (*(Entry* const*)a)->time;
}

int compareNameReverse(const void* a, const void* b) {
    return -compareName(a, b);
}

int compareSizeReverse(const void* a, const void* b) {
    return -compareSize(a, b);
}

int compareTimeReverse(const void* a, const void* b) {
    return -compareTime(a, b);
}

size_t formatSize(size_t size, bool si, char* unit) {
    static const char units[] = {'B', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y'};
    size_t base = si ? 1000 : 1024;
    size_t n = 0;
    while (size > base) {
        size /= base;
        n++;
    }
    *unit = units[n];
    return size;
}

void listPath(const char* path, Arguments* args) {
    List entries;
    initList(&entries);
    struct stat stats;
    if (stat(path, &stats) != 0) {
        fprintf(stderr, "%s: cannot access '%s': %s\n", args->prog, path, strerror(errno));
        return;
    }
    if (!args->directory && (stats.st_mode & S_IFMT) == S_IFDIR) {
        DIR* dir = opendir(path);
        struct dirent* entr = readdir(dir);
        while (entr != NULL) {
            if (
                entr->d_name[0] != '.'
                || args->filter == FILTER_ALL
                || (args->filter == FILTER_ALMOST_ALL
                    && strcmp(entr->d_name, ".") != 0
                    && strcmp(entr->d_name, "..") != 0)
            ) {
                char* entr_path = joinPaths(path, entr->d_name);
                Entry* entry = entryForPath(entr_path, args);
                addToList(&entries, entry);
                if (args->recursive && (entry->mode & S_IFMT) == S_IFDIR) {
                    addToList(&args->files, entr_path);
                } else {
                    free(entr_path);
                }
            }
            entr = readdir(dir);
        }
        closedir(dir);
    } else {
        addToList(&entries, entryForPath(path, args));
    }
    if (args->sort == SORT_NAME) {
        qsort(entries.values, entries.count, sizeof(Entry*), args->reverse ? compareSizeReverse : compareName);
    } else if (args->sort == SORT_SIZE) {
        qsort(entries.values, entries.count, sizeof(Entry*), args->reverse ? compareSizeReverse : compareSize);
    } else if (args->sort == SORT_TIME) {
        qsort(entries.values, entries.count, sizeof(Entry*), args->reverse ? compareTimeReverse : compareTime);
    }
    if (args->files.count > 1 && entries.count > 0) {
        printf("%s:\n", path);
    }
    size_t max_nlink = 0;
    size_t max_size = 0;
    struct tm* cur_tm = NULL;
    if (args->long_fmt) {
        time_t cur_time = time(NULL);
        const struct tm* tm = localtime(&cur_time);
        cur_tm = malloc(sizeof(struct tm));
        memcpy(cur_tm, tm, sizeof(struct tm));
        for (size_t i = 0; i < entries.count; i++) {
            Entry* entry = LIST_GET(Entry*, entries, i);
            if (entry->nlink > max_nlink) {
                max_nlink = entry->nlink;
            }
            if (args->format == FORMAT_NONE) {
                if ((size_t)entry->size > max_size) {
                    max_size = entry->size;
                }
            } else {
                char unit;
                size_t fmt_size = formatSize(entry->size, args->format == FORMAT_SI, &unit);
                if (fmt_size > max_size) {
                    max_size = fmt_size;
                }
            }
        }
    }
    for (size_t i = 0; i < entries.count; i++) {
        Entry* entry = LIST_GET(Entry*, entries, i);
        if (args->long_fmt) {
            char mode[10];
            if ((entry->mode & S_IFMT) == S_IFBLK) {
                mode[0] = 'b';
            } else if ((entry->mode & S_IFMT) == S_IFCHR) {
                mode[0] = 'c';
            } else if ((entry->mode & S_IFMT) == S_IFDIR) {
                mode[0] = 'd';
            } else if ((entry->mode & S_IFMT) == S_IFIFO) {
                mode[0] = 'f';
            } else if ((entry->mode & S_IFMT) == S_IFLNK) {
                mode[0] = 'l';
            } else {
                mode[0] = '-';
            }
            for (size_t i = 0; i < 3; i++) {
                mode_t grp = (entry->mode >> (3 * i));
                mode[9 - 3 * i] = (grp & 0b001) != 0 ? 'x' : '-';
                mode[8 - 3 * i] = (grp & 0b010) != 0 ? 'w' : '-';
                mode[7 - 3 * i] = (grp & 0b100) != 0 ? 'r' : '-';
            }
            const struct tm* tm = localtime(&entry->time);
            char time[256];
            if (cur_tm->tm_year != tm->tm_year) {
                strftime(time, sizeof(time), "%b %e  %Y", tm);
            } else {
                strftime(time, sizeof(time), "%b %e %H:%M", tm);
            }
            if (args->format == FORMAT_NONE) {
                printf(
                    "%s %*hu %*lu %s %s\n", mode, decimalWidth(max_nlink), entry->nlink,
                    decimalWidth(max_size), entry->size, time, entry->name
                );
            } else {
                char unit;
                size_t fmt_size = formatSize(entry->size, args->format == FORMAT_SI, &unit);
                printf(
                    "%s %*hu %*lu%c %s %s\n", mode, decimalWidth(max_nlink), entry->nlink,
                    decimalWidth(max_size), fmt_size, unit, time, entry->name
                );
            }
        } else {
            printf("%s", entry->name);
            if (i == entries.count - 1) {
                printf("\n");
            } else {
                printf("  ");
            }
        }
        free(entry->name);
        free(entry);
    }
    deinitList(&entries);
}

int main(int argc, const char* const* argv) {
    Arguments args = {
        .prog = argv[0],
        .filter = FILTER_DEFAULT,
        .sort = SORT_NAME,
        .format = FORMAT_NONE,
        .recursive = false,
        .directory = false,
        .long_fmt = false,
        .reverse = false,
    };
    initList(&args.files);
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    for (size_t i = 0; i < args.files.count; i++) {
        char* path = LIST_GET(char*, args.files, i);
        if (i != 0) {
            printf("\n");
        }
        listPath(path, &args);
        free(path);
    }
    deinitList(&args.files);
    return 0;
}

