
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
    bool no_clobber;
    bool no_dir;
    bool update;
    bool link;
    bool recursive;
    bool verbose;
    bool error;
    char* target;
    List files;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*,
    "cp [options] [-T] <source> <dest>\n"
    "  or:  cp [options] <source>... <directory>\n"
    "  or:  cp [options] -t <directory> <source>..."
, {
    // Options
    ARG_FLAG('l', "link", {
        context->link = true;
    }, "hard link files instead of copying");
    ARG_FLAG('n', "no-clobber", {
        context->no_clobber = true;
    }, "do not overwrite an existing file");
    ARG_FLAG('R', NULL, {
        context->recursive = true;
    }, "");
    ARG_FLAG('r', "recursive", {
        context->recursive = true;
    }, "copy directories and their contents recursively");
    ARG_VALUED('t', "target-directory", {
        free(context->target);
        context->target = strdup(value);
    }, false, "=<directory>", "copy all sources into the directory");
    ARG_FLAG('T', "no-target-directory", {
        context->no_dir = true;
    }, "treat the destination as a normal file");
    ARG_FLAG('u', "update", {
        context->update = true;
    }, "copy only if the source is newer that the destination");
    ARG_FLAG('v', "verbose", {
        context->verbose = true;
    }, "explain what is being done");
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
    if (context->target == NULL && context->files.count > 0) {
        context->files.count--;
        context->target = context->files.values[context->files.count];
    }
    if (context->files.count == 0) {
        const char* option = NULL;
        ARG_WARN("missing file operand");
    } else if (context->no_dir && context->files.count > 1) {
        const char* option = context->files.values[1];
        ARG_WARN("extra operand");
    }
})

static void copyPath(const char* src, const char* dst, Arguments* args);

static void copyFile(const char* src, const char* dst, Arguments* args) {
    struct stat src_stat;
    if (stat(src, &src_stat) != 0) {
        args->error = true;
        fprintf(stderr, "%s: cannot stat '%s': %s\n", args->prog, src, strerror(errno));
        return;
    }
    struct stat dst_stat;
    bool dst_exists = stat(dst, &dst_stat) == 0;
    if (
        (!args->no_clobber || !dst_exists)
        && (!args->update || src_stat.st_mtime > dst_stat.st_mtime)
    ) {
        if (args->link) {
            if (dst_exists) {
                if (remove(dst) != 0) {
                    args->error = true;
                    fprintf(stderr, "%s: cannot remove '%s': %s\n", args->prog, dst, strerror(errno));
                    return;
                }
            }
            if (link(src, dst) != 0) {
                args->error = true;
                fprintf(stderr, "%s: cannot link '%s' to '%s': %s\n", args->prog, src, dst, strerror(errno));
                return;
            }
            if (args->verbose) {
                printf("linked '%s' to '%s'\n", src, dst);
            }
        } else {
            size_t tmp_size;
            char buffer[1024];
            int fsrc = open(src, O_RDONLY);
            if (fsrc < 0) {
                args->error = true;
                fprintf(stderr, "%s: cannot open '%s': %s\n", args->prog, src, strerror(errno));
                return;
            }
            if (dst_exists) {
                if (remove(dst) != 0) {
                    args->error = true;
                    fprintf(stderr, "%s: cannot remove '%s': %s\n", args->prog, dst, strerror(errno));
                    return;
                }
            }
            int fdst = open(dst, O_CREAT | O_TRUNC | O_WRONLY, src_stat.st_mode);
            if (fdst < 0) {
                args->error = true;
                fprintf(stderr, "%s: cannot open '%s': %s\n", args->prog, dst, strerror(errno));
                return;
            }
            do {
                tmp_size = read(fsrc, buffer, 1024);
                size_t left = tmp_size;
                while (left > 0) {
                    left -= write(fdst, buffer, tmp_size);
                }
            } while (tmp_size != 0);
            close(fsrc);
            close(fdst);
            if (args->verbose) {
                printf("copied '%s' to '%s'\n", src, dst);
            }
        }
    }
}

static void copyDirectory(const char* src, const char* dst, Arguments* args) {
    if (args->recursive) {
        struct stat src_stat;
        if (stat(src, &src_stat) != 0) {
            args->error = true;
            fprintf(stderr, "%s: cannot stat '%s': %s\n", args->prog, src, strerror(errno));
            return;
        }
        struct stat dst_stat;
        bool dst_exists = stat(dst, &dst_stat) == 0;
        if (dst_exists && (dst_stat.st_mode & S_IFMT) != S_IFDIR) {
            args->error = true;
            fprintf(stderr, "%s: cannot copy '%s' to '%s': %s\n", args->prog, src, dst, strerror(ENOTDIR));
            return;
        } else if (!dst_exists) {
            if (mkdir(dst, src_stat.st_mode) != 0) {
                args->error = true;
                fprintf(stderr, "%s: cannot mkdir '%s': %s\n", args->prog, src, strerror(ENOTDIR));
                return;
            }
            if (args->verbose) {
                printf("created directory '%s'\n", dst);
            }
        }
        DIR* dir = opendir(src);
        if (dir == NULL) {
            args->error = true;
            fprintf(stderr, "%s: cannot open '%s': %s\n", args->prog, src, strerror(errno));
            return;
        }
        struct dirent* entr = readdir(dir);
        List children_src;
        initList(&children_src);
        List children_dst;
        initList(&children_dst);
        while (entr != NULL) {
            if (strcmp(entr->d_name, ".") != 0 && strcmp(entr->d_name, "..") != 0) {
                char* entr_src_path = joinPaths(src, entr->d_name);
                copyStringToList(&children_src, entr_src_path);
                char* entr_dst_path = joinPaths(dst, entr->d_name);
                copyStringToList(&children_dst, entr_dst_path);
            }
            entr = readdir(dir);
        }
        closedir(dir);
        for (size_t i = 0; i < children_src.count; i++) {
            char* entr_src_path = LIST_GET(char*, children_src, i);
            char* entr_dst_path = LIST_GET(char*, children_dst, i);
            copyPath(entr_src_path, entr_dst_path, args);
            free(entr_src_path);
            free(entr_dst_path);
        }
        deinitList(&children_src);
        deinitList(&children_dst);
    } else {
        args->error = true;
        fprintf(stderr, "%s: cannot copy '%s': %s\n", args->prog, src, strerror(EISDIR));
        return;
    }
}

static void copyPath(const char* src, const char* dst, Arguments* args) {
    struct stat stats;
    if (stat(src, &stats) != 0) {
        args->error = true;
        fprintf(stderr, "%s: cannot stat '%s': %s\n", args->prog, src, strerror(errno));
        return;
    }
    if ((stats.st_mode & S_IFMT) == S_IFDIR) {
        copyDirectory(src, dst, args);
    } else if ((stats.st_mode & S_IFMT) == S_IFREG) {
        copyFile(src, dst, args);
    }
}

int main(int argc, const char* const* argv) {
    Arguments args;
    args.prog = argv[0];
    args.no_clobber = false;
    args.no_dir = false;
    args.update = false;
    args.link = false;
    args.recursive = false;
    args.verbose = false;
    args.error = false;
    args.target = NULL;
    initList(&args.files);
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    bool is_dir = false;
    struct stat dst;
    if (stat(args.target, &dst) != 0) {
        if (errno != ENOENT && errno != ENOTDIR) {
            fprintf(stderr, "%s: target '%s': %s\n", args.prog, args.target, strerror(errno));
            exit(1);
        }
    } else {
        is_dir = (dst.st_mode & S_IFMT) == S_IFDIR;
    }
    if (is_dir && !args.no_dir) {
        for (size_t i = 0; i < args.files.count; i++) {
            char* path = LIST_GET(char*, args.files, i);
            char* new_path = joinPaths(args.target, basename(path));
            copyPath(path, new_path, &args);
            free(new_path);
        }
    } else {
        if (args.files.count > 1) {
            fprintf(stderr, "%s: target '%s': %s\n", args.prog, args.target, strerror(ENOTDIR));
            exit(1);
        }
        char* path = LIST_GET(char*, args.files, 0);
        copyPath(path, args.target, &args);
    }
    deinitListAndContents(&args.files);
    free(args.target);
    return args.error ? 1 : 0;
}

