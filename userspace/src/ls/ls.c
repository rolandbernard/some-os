
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "args.h"
#include "stringlist.h"

typedef struct {
    StringList files;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "ls [options] [file]...", {
    // Options
    ARG_FLAG('a', "all", {
    }, "do not ignore entries starting with .");
    ARG_FLAG('A', "--almost-all", {
    }, "do not list . and ..");
    ARG_FLAG('d', "directory", {
    }, "list directories themselves, not their contents");
    ARG_FLAG('f', NULL, {
    }, "list all entries in directory order");
    ARG_FLAG('h', "human-readable", {
    }, "with -l, print sizes like 1K 234M 2G etc.");
    ARG_FLAG(0, "si", {
    }, "likewise, but use powers of 1000 not 1024");
    ARG_FLAG('l', NULL, {
    }, "use long listing format");
    ARG_FLAG('r', "reverse", {
    }, "reverse order while sorting");
    ARG_FLAG('R', "recursive", {
    }, "list subdirectories recursively");
    ARG_FLAG('S', NULL, {
    }, "sort by file size, largest first");
    ARG_VALUED(0, "sort", {
    }, false, "={none|size|time}", "select sorting mode (-U, -S, -t)");
    ARG_FLAG('t', NULL, {
    }, "sort by time, newest first");
    ARG_FLAG('U', NULL, {
    }, "list all entries in directory order");
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
})

int main(int argc, const char* const* argv) {
    Arguments args;
    initStringList(&args.files);
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    for (size_t i = 0; i < args.files.count; i++) {
        DIR* dir = opendir(args.files.strings[i]);
        struct dirent* entr = readdir(dir);
        while (entr != NULL) {
            printf("%s ", entr->d_name);
            entr = readdir(dir);
        }
        closedir(dir);
        printf("\n");
    }
    deinitStringList(&args.files);
    return 0;
}

