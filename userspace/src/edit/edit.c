
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
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
    const char* path;
    const char* status;
    TextLine* lines;
    size_t line_count;
    size_t cursor_row;
    size_t cursor_column;
    size_t view_row;
    size_t view_column;
    size_t width;
    size_t height;
    bool changed;
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
    newterm.c_cc[VMIN] = 1;
    newterm.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, 0, &newterm);
    setvbuf(stdout, malloc(1 << 16), _IOFBF, 1 << 16);
    printf("\e7\e[6 q\e[?47h\e[2J\e[H");
    fflush(stdout);
}

static void restoreDisplay() {
    printf("\e[?25h\e[?47l\e[ q\e8");
    fflush(stdout);
    tcsetattr(STDIN_FILENO, 0, &oldterm);
}

static void getTerminalSize(EditorState* state) {
    printf("\e[999;999H\e[6n\e[%lu;%luH", state->cursor_row + 1, state->cursor_column + 1);
    fflush(stdout);
    scanf("\e[%lu;%luR", &state->height, &state->width);
}

static void moveView(EditorState* state) {
    int line_nums = decimalWidth(state->line_count) + 2;
    if (line_nums < 4) {
        line_nums = 4;
    }
    if (state->cursor_column < state->view_column) {
        state->view_column = state->cursor_column;
    }
    if (state->cursor_column - state->view_column >= state->width - line_nums - 1) {
        state->view_column = state->cursor_column - state->width + line_nums + 1;
    }
    if (state->cursor_row < state->view_row) {
        state->view_row = state->cursor_row;
    }
    if (state->cursor_row - state->view_row >= state->height - 2) {
        state->view_row = state->cursor_row - state->height + 2;
    }
}

static void displayEditor(EditorState* state) {
    printf("\e[?25l");
    getTerminalSize(state);
    moveView(state);
    printf("\e[H");
    int line_nums = decimalWidth(state->line_count) + 2;
    if (line_nums < 4) {
        line_nums = 4;
    }
    for (size_t i = 0; i < state->height - 1; i++) {
        size_t row = state->view_row + i;
        if (row < state->line_count) {
            printf("\e[90m%*lu\e[m %c", line_nums - 2, row + 1, state->view_column != 0 ? '<' : ' ');
            for (size_t j = 0; j < state->width - line_nums; j++) {
                size_t col = state->view_column + j;
                if (col < state->lines[row].length) {
                    if (j == state->width - line_nums - 1) {
                        fputc('>', stdout);
                    } else {
                        fputc(state->lines[row].text[col], stdout);
                    }
                } else {
                    break;
                }
            }
        }
        printf("\e[K\n");
    }
    printf(
        " %*lu:%lu  %s  %s", decimalWidth(state->line_count), state->cursor_row + 1,
        state->cursor_column + 1, state->path, strerror(errno)
    );
    if (strlen(state->status) != 0) {
        printf(" (%s)", state->status);
    }
    printf(
        "\e[K\e[%lu;%luH\e[?25h", state->cursor_row - state->view_row + 1,
        state->cursor_column - state->view_column + 1 + line_nums
    );
    fflush(stdout);
}

static char* readLineFromFile(FILE* file) {
    size_t length = 0;
    char* buffer = NULL;
    while (!feof(file)) {
        buffer = realloc(buffer, length + 1024);
        if (fgets(buffer + length, 1024, file) == NULL) {
            buffer[0] = 0;
        }
        if (buffer[0] != 0 && buffer[strlen(buffer) - 1] == '\n') {
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

static void loadFile(EditorState* state) {
    FILE* file = fopen(state->path, "r");
    if (file == NULL && errno == ENOENT) {
        file = fopen(state->path, "w+");
        state->status = "created a new file";
    }
    if (file == NULL) {
        fprintf(stderr, "%s: cannot open '%s': %s\n", args.prog, state->path, strerror(errno));
        exit(1);
    }
    while (!feof(file)) {
        char* line = readLineFromFile(file);
        if (line != NULL) {
            insertNewLine(state, state->line_count, line);
        }
    }
    fclose(file);
    if (state->line_count == 0) {
        insertNewLine(state, 0, strdup(""));
    }
}

static void writeFile(EditorState* state) {
    FILE* file = fopen(state->path, "w");
    if (file == NULL) {
        state->status = "failed to save";
        return;
    }
    for (size_t i = 0; i < state->line_count; i++) {
        fwrite(state->lines[i].text, 1, state->lines[i].length, file);
        fputc('\n', file);
    }
    fclose(file);
    state->status = "saved";
    errno = 0;
}

#define C(CHAR) (CHAR + 1 - 'A')
#define DEL 0x7f
#define UP 'A'
#define DOWN 'B'
#define RIGHT 'C'
#define LEFT 'D'

static void insertText(EditorState* state, size_t len, const char* text) {
    TextLine* line = &state->lines[state->cursor_row];
    line->text = realloc(line->text, line->length + len);
    memmove(
        line->text + state->cursor_column + len, line->text + state->cursor_column,
        line->length - state->cursor_column
    );
    memcpy(line->text + state->cursor_column, text, len);
    state->cursor_column += len;
    line->length += len;
}

static bool isInputAvail() {
    int num;
    ioctl(STDIN_FILENO, FIONREAD, &num);
    return num != 0;
}

static int readChar() {
    char c;
    while (read(STDIN_FILENO, &c, 1) != 1);
    return c;
}

static void changedState(EditorState* state) {
    state->changed = true;
}

static bool handleInput(EditorState* state) {
    do {
        int c = readChar();
        if (c == C('S') || c == C('Y')) {
            writeFile(state);
            changedState(state);
        } else if (c == '\b' || c == DEL) {
            if (state->cursor_column == 0) {
                if (state->cursor_row != 0) {
                    state->cursor_row--;
                    TextLine* prev = &state->lines[state->cursor_row];
                    TextLine* next = &state->lines[state->cursor_row + 1];
                    prev->text = realloc(prev->text, prev->length + next->length);
                    memcpy(prev->text + prev->length, next->text, next->length);
                    state->cursor_column = prev->length;
                    prev->length += next->length;
                    free(next->text);
                    memmove(next, next + 1, (state->line_count - state->cursor_row - 2) * sizeof(TextLine));
                    state->line_count--;
                    changedState(state);
                }
            } else {
                state->cursor_column--;
                TextLine* line = &state->lines[state->cursor_row];
                memmove(
                    line->text + state->cursor_column, line->text + state->cursor_column + 1,
                    line->length - state->cursor_column - 1
                );
                line->length--;
                changedState(state);
            }
        } else if (c == '\n') {
            state->cursor_row++;
            state->lines = realloc(state->lines, (state->line_count + 1) * sizeof(TextLine));
            TextLine* prev = &state->lines[state->cursor_row - 1];
            TextLine* next = &state->lines[state->cursor_row];
            memmove(next + 1, next, (state->line_count - state->cursor_row) * sizeof(TextLine));
            next->length = prev->length - state->cursor_column;
            next->text = malloc(next->length);
            prev->length -= next->length;
            memcpy(next->text, prev->text + prev->length, next->length);
            state->cursor_column = 0;
            state->line_count++;
            changedState(state);
        } else if (c == '\t') {
            insertText(state, 4, "    ");
            changedState(state);
        } else if (isprint(c)) {
            char str[1] = { c };
            insertText(state, 1, str);
            changedState(state);
        } else if (c == '\e' && readChar() == '[') {
            c = readChar();
            if (c == RIGHT) {
                if (state->cursor_column < state->lines[state->cursor_row].length) {
                    state->cursor_column++;
                    changedState(state);
                }
            } else if (c == LEFT) {
                if (state->cursor_column > 0) {
                    state->cursor_column--;
                    changedState(state);
                }
            } else if (c == UP) {
                if (state->cursor_row > 0) {
                    state->cursor_row--;
                    if (state->lines[state->cursor_row].length < state->cursor_column) {
                        state->cursor_column = state->lines[state->cursor_row].length;
                    }
                    changedState(state);
                }
            } else if (c == DOWN) {
                if (state->cursor_row < state->line_count - 1) {
                    state->cursor_row++;
                    if (state->lines[state->cursor_row].length < state->cursor_column) {
                        state->cursor_column = state->lines[state->cursor_row].length;
                    }
                    changedState(state);
                }
            }
            while (!isalpha(c)) {
                c = readChar();
            }
        } else if (c == C('C') || c == C('D') || c == C('X')) {
            return true;
        }
    } while (isInputAvail());
    return false;
}

int main(int argc, const char* const* argv) {
    args.prog = argv[0];
    args.file = NULL;
    ARG_PARSE_ARGS(argumentSpec, argc, argv, &args);
    EditorState state = {
        .path = args.file,
        .status = "",
        .lines = NULL,
        .line_count = 0,
        .cursor_column = 0,
        .cursor_row = 0,
        .view_column = 0,
        .view_row = 0,
        .width = 999,
        .height = 999,
        .changed = true,
    };
    loadFile(&state);
    atexit(restoreDisplay);
    setupDisplay();
    for (;;) {
        if (state.changed) {
            displayEditor(&state);
            state.changed = false;
        }
        if (handleInput(&state)) {
            exit(0);
        }
    }
    return 0;
}

