
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "args.h"
#include "list.h"
#include "util.h"

typedef struct {
    const char* prog;
    const char* file;
} Arguments;

ARG_SPEC_FUNCTION(argumentSpec, Arguments*, "edit <file>", {
    // Options
    ARG_FLAG(0, "help", {
        ARG_PRINT_HELP(argumentSpec, NULL);
        exit(0);
    }, "display this help and exit");
}, {
    // Default
    if (context->file == NULL) {
        context->file = value;
    } else {
        const char* option = value;
        ARG_WARN("extra operand");
    }
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
    if (context->file == NULL) {
        const char* option = NULL;
        ARG_WARN("missing file operand");
    }
})

static Arguments args;

typedef struct {
    char* text;
    size_t length;
} TextLine;

typedef struct {
    TextLine* lines;
    size_t line_count;
    size_t cursor_row;
    size_t cursor_column;
} EditorState;

static void insertNewLine(EditorState* state, size_t pos, char* line) {
    state->lines = realloc(state->lines, (1 + state->line_count) * sizeof(TextLine));
    memmove(state->lines + pos + 1, state->lines + pos, (state->line_count - pos) * sizeof(TextLine));
    state->line_count++;
    state->lines[pos].text = line;
    state->lines[pos].length = strlen(line);
}

static struct termios oldterm;

static void setupDisplay() {
    struct termios newterm;
    tcgetattr(STDIN_FILENO, &oldterm);
    newterm = oldterm;
    newterm.c_lflag &= ~(ICANON | ECHO | ISIG);
    newterm.c_cc[VMIN] = 0;
    newterm.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, 0, &newterm);
    printf("\e7\e[6 q\e[?47h\e[2J\e[H");
}

static void restoreDisplay() {
    printf("\e[?47l\e[ q\e8");
    tcsetattr(STDIN_FILENO, 0, &oldterm);
}

static void displayEditor(EditorState* state) {

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
    if (feof(file) && length == 0) {
        free(buffer);
        return NULL;
    } else {
        return buffer;
    }
}

static void loadFile(EditorState* state, const char* path) {
    FILE* file = fopen(path, "r");
    if (file == NULL) {
        if (errno != ENOENT) {
            fprintf(stderr, "%s: cannot open '%s': %s\n", args.prog, path, strerror(errno));
            exit(1);
        }
    } else {
        while (!feof(file)) {
            char* line = readLineFromFile(file);
            if (line != NULL) {
                insertNewLine(state, state->line_count, line);
            }
        }
        fclose(file);
    }
}

static void writeFile(EditorState* state, const char* path) {

}

static bool handleChar(EditorState* state, char c) {
    return c == 4;
}

static void waitForInput() {
    fd_set read;
    FD_ZERO(&read);
    FD_SET(STDIN_FILENO, &read);
    fd_set write;
    FD_ZERO(&write);
    fd_set error;
    FD_ZERO(&error);
    select(STDIN_FILENO + 1, &read, &write, &error, NULL);
}

int main(int argc, const char* const* argv) {
    args.prog = argv[0];
    args.file = NULL;
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    EditorState state = {
        .lines = NULL,
        .line_count = 0,
        .cursor_column = 0,
        .cursor_row = 0,
    };
    loadFile(&state, args.file);
    atexit(restoreDisplay);
    setupDisplay();
    for (;;) {
        waitForInput();
        char buffer[512];
        size_t len = read(STDIN_FILENO, buffer, 512);
        for (size_t i = 0; i < len; i++) {
            if (handleChar(&state, buffer[i])) {
                break;
            }
        }
        displayEditor(&state);
    }
    restoreDisplay();
    writeFile(&state, args.file);
    return 0;
}

