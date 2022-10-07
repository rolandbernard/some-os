
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* prog;

static bool isZero(const char* arg) {
    if (arg == NULL || strlen(arg) == 0) {
        return true;
    }
    if (*arg == '-') {
        arg++;
    }
    while (*arg != 0) {
        if (*arg != '0') {
            return false;
        }
        arg++;
    }
    return true;
}

static bool isInteger(const char* arg) {
    if (arg == NULL) {
        return false;
    }
    if (*arg == '-') {
        arg++;
    }
    do {
        if (*arg < '0' || *arg > '9') {
            return false;
        }
        if (*arg != 0) {
            arg++;
        }
    } while (*arg != 0);
    return true;
}

static void assertInteger(const char* arg) {
    if (!isInteger(arg)) {
        fprintf(stderr, "%s: non-integer argument", prog);
        exit(2);
    }
}

static void syntaxError(const char* exp, const char* found) {
    fprintf(stderr, "%s: syntax error: expected %s found %s", prog, exp, found);
    exit(2);
}

static int cmpInteger(const char* fst, const char* snd) {
    if (*fst == '-' && *snd == '-') {
        return -cmpInteger(fst + 1, snd + 1);
    } else if (*fst == '-') {
        return -1;
    } else if (*snd == '-') {
        return 1;
    } else {
        while (*fst == '0') {
            fst++;
        }
        while (*snd == '0') {
            snd++;
        }
        if (strlen(fst) != strlen(snd)) {
            return strlen(fst) - strlen(snd);
        } else {
            return strcmp(fst, snd);
        }
    }
}

static char* subInteger(const char* fst, const char* snd);

static char* negInteger(const char* arg) {
    if (*arg == '-') {
        return strdup(arg + 1);
    } else {
        char* res = malloc(strlen(arg) + 2);
        strcat("-", arg);
        return res;
    }
}

static char* addInteger(const char* fst, const char* snd) {
    if (*fst == '-' && *snd == '-') {
        char* tmp = addInteger(fst + 1, snd + 1);
        char* res = negInteger(tmp);
        free(tmp);
        return res;
    } else if (*fst == '-') {
        if (cmpInteger(fst + 1, snd) > 0) {
            char* tmp = subInteger(fst + 1, snd);
            char* res = negInteger(tmp);
            free(tmp);
            return res;
        } else {
            return subInteger(snd, fst + 1);
        }
    } else if (*snd == '-') {
        if (cmpInteger(fst, snd + 1) < 0) {
            char* tmp = subInteger(snd + 1, fst);
            char* res = negInteger(tmp);
            free(tmp);
            return res;
        } else {
            return subInteger(fst, snd + 1);
        }
    } else {
        while (*fst == '0') {
            fst++;
        }
        while (*snd == '0') {
            snd++;
        }
        if (strlen(fst) < strlen(snd)) {
            const char* tmp = snd;
            snd = fst;
            fst = tmp;
        }
        char* res = malloc(strlen(fst) + 3);
        res[0] = '0';
        strcpy(res + 1, fst);
        size_t fst_len = strlen(fst);
        size_t snd_len = strlen(snd);
        size_t pos = 0;
        int carry = 0;
        do {
            int v = carry;
            if (pos < fst_len) {
                v += fst[fst_len - 1 - pos] - '0';
            }
            if (pos < snd_len) {
                v += snd[snd_len - 1 - pos] - '0';
            }
            res[fst_len - pos] = (v % 10) + '0';
            carry = v / 10;
            pos++;
        } while (pos <= fst_len);
        while (*res == '0') {
            memmove(res, res + 1, strlen(res) + 1);
        }
        return res;
    }
}

static char* subInteger(const char* fst, const char* snd) {
    // TODO
    return strdup(fst);
}

typedef struct {
    const char* const* args;
    size_t count;
    size_t off;
} Scanner;

static const char* peek(Scanner* args) {
    if (args->off < args->count) {
        return args->args[args->off];
    } else {
        return NULL;
    }
}

static const char* next(Scanner* args) {
    if (args->off < args->count) {
        return args->args[args->off++];
    } else {
        return NULL;
    }
}

static bool eq(const char* fst, const char* snd) {
    if (fst == NULL || snd == NULL) {
        return fst == snd;
    } else {
        return strcmp(fst, snd) == 0;
    }
}

static char* evalExpr(Scanner* args);

static char* evalBase(Scanner* args) {
    const char* arg = next(args);
    if (eq(arg, "(")) {
        char* res = evalExpr(args);
        arg = next(args);
        if (!eq(arg, ")")) {
            syntaxError(")", arg);
        }
        return res;
    } else {
        return strdup(arg);
    }
}

static char* evalPre(Scanner* args) {
    // TODO
    return evalBase(args);
}

static char* evalMul(Scanner* args) {
    // TODO
    return evalPre(args);
}

static char* evalAdd(Scanner* args) {
    char* res = evalMul(args);
    while (peek(args) != NULL && (eq(peek(args), "+") || eq(peek(args), "-"))) {
        const char* op = next(args);
        char* fst = res;
        char* snd = evalMul(args);
        assertInteger(res);
        assertInteger(snd);
        if (eq(op, "+")) {
            res = addInteger(fst, snd);
        } else if (eq(op, "-")) {
            res = subInteger(fst, snd);
        }
        free(fst);
        free(snd);
    }
    return res;
}

static char* evalComp(Scanner* args) {
    char* res = evalAdd(args);
    while (
        peek(args) != NULL
        && (
            eq(peek(args), "<") || eq(peek(args), "<=") 
            || eq(peek(args), "=") || eq(peek(args), "!=")
            || eq(peek(args), ">=") || eq(peek(args), ">")
        )
    ) {
        const char* op = next(args);
        char* snd = evalAdd(args);
        assertInteger(res);
        assertInteger(snd);
        int cmp = cmpInteger(res, snd);
        free(res);
        free(snd);
        if (eq(op, "<")) {
            res = strdup(cmp < 0 ? "1" : "0");
        } else if (eq(op, "<=")) {
            res = strdup(cmp <= 0 ? "1" : "0");
        } else if (eq(op, "=")) {
            res = strdup(cmp == 0 ? "1" : "0");
        } else if (eq(op, "!=")) {
            res = strdup(cmp != 0 ? "1" : "0");
        } else if (eq(op, ">=")) {
            res = strdup(cmp >= 0 ? "1" : "0");
        } else if (eq(op, ">")) {
            res = strdup(cmp > 0 ? "1" : "0");
        }
    }
    return res;
}

static char* evalAndOr(Scanner* args) {
    char* res = evalComp(args);
    while (peek(args) != NULL && (eq(peek(args), "|") || eq(peek(args), "&"))) {
        const char* op = next(args);
        char* snd = evalComp(args);
        if (eq(op, "|")) {
            if (isZero(res)) {
                free(res);
                res = snd;
            }
        } else if (eq(op, "&")) {
            if (isZero(res) || isZero(snd)) {
                free(res);
                free(snd);
                res = strdup("0");
            }
        }
    }
    return res;
}

static char* evalExpr(Scanner* args) {
    return evalAndOr(args);
}

int main(int argc, const char* const* argv) {
    prog = argv[0];
    if (argc <= 1) {
        fprintf(stderr, "%s: missing operand", prog);
        exit(2);
    }
    Scanner scanner = {
        .args = argv,
        .count = argc,
        .off = 1,
    };
    char* result = evalExpr(&scanner);
    printf("%s\n", result);
    return isZero(result) ? 1 : 0;
}

