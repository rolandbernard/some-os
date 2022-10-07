
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
        fprintf(stderr, "%s: non-integer argument\n", prog);
        exit(2);
    }
}

static void syntaxError(const char* exp, const char* found) {
    fprintf(stderr, "%s: syntax error: expected %s found %s\n", prog, exp, found);
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

static char* negInline(char* arg) {
    if (*arg == '-') {
        memmove(arg, arg + 1, strlen(arg));
        return arg;
    } else {
        arg = realloc(arg, strlen(arg) + 2);
        memmove(arg + 1, arg, strlen(arg) + 1);
        arg[0] = '-';
        return arg;
    }
}

static char* addInteger(const char* fst, const char* snd) {
    if (*fst == '-' && *snd == '-') {
        return negInline(addInteger(fst + 1, snd + 1));
    } else if (*fst == '-') {
        if (cmpInteger(fst + 1, snd) > 0) {
            return negInline(subInteger(fst + 1, snd));
        } else {
            return subInteger(snd, fst + 1);
        }
    } else if (*snd == '-') {
        if (cmpInteger(fst, snd + 1) < 0) {
            return negInline(subInteger(snd + 1, fst));
        } else {
            return subInteger(fst, snd + 1);
        }
    } else if (cmpInteger(fst, snd) < 0) {
        return addInteger(snd, fst);
    } else {
        while (*fst == '0') {
            fst++;
        }
        while (*snd == '0') {
            snd++;
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
        char* non_zero = res;
        while (*non_zero == '0') {
            non_zero++;
        }
        if (strlen(non_zero) == 0) {
            non_zero--;
        }
        memmove(res, non_zero, strlen(non_zero) + 1);
        return res;
    }
}

static char* subInteger(const char* fst, const char* snd) {
    if (*fst == '-' && *snd == '-') {
        return subInteger(snd + 1, fst + 1);
    } else if (*fst == '-') {
        return negInline(addInteger(fst + 1, snd));
    } else if (*snd == '-') {
        return addInteger(fst, snd + 1);
    } else if (cmpInteger(fst, snd) < 0) {
        return negInline(subInteger(snd, fst));
    } else {
        while (*fst == '0') {
            fst++;
        }
        while (*snd == '0') {
            snd++;
        }
        char* res = malloc(strlen(fst) + 2);
        strcpy(res, fst);
        size_t fst_len = strlen(fst);
        size_t snd_len = strlen(snd);
        size_t pos = 0;
        int carry = 0;
        do {
            int val = fst[fst_len - 1 - pos] - '0';
            int sub = carry;
            if (pos < snd_len) {
                sub += snd[snd_len - 1 - pos] - '0';
            }
            carry = 0;
            while (sub > val) {
                carry++;
                val += 10;
            }
            res[fst_len - 1 - pos] = val - sub + '0';
            pos++;
        } while (pos < fst_len);
        char* non_zero = res;
        while (*non_zero == '0') {
            non_zero++;
        }
        if (strlen(non_zero) == 0) {
            non_zero--;
        }
        memmove(res, non_zero, strlen(non_zero) + 1);
        return res;
    }
}

static char* decShiftInline(char* arg, size_t pow) {
    arg = realloc(arg, strlen(arg) + pow + 1);
    size_t old_len = strlen(arg);
    for (size_t i = 0; i < pow; i++) {
        arg[old_len + i] = '0';
    }
    arg[old_len + pow] = 0;
    return arg;
}

static char* mulInteger(const char* fst, const char* snd) {
    if (*fst == '-' && *snd == '-') {
        return mulInteger(fst + 1, snd + 1);
    } else if (*fst == '-') {
        return negInline(mulInteger(fst + 1, snd));
    } else if (*snd == '-') {
        return negInline(mulInteger(fst, snd + 1));
    } else if (cmpInteger(fst, snd) < 0) {
        return mulInteger(snd, fst);
    } else {
        while (*fst == '0') {
            fst++;
        }
        while (*snd == '0') {
            snd++;
        }
        if (strlen(snd) == 1) {
            char* res = malloc(strlen(fst) + 3);
            res[0] = '0';
            strcpy(res + 1, fst);
            size_t fst_len = strlen(fst);
            size_t pos = 0;
            int carry = 0;
            do {
                int v = carry;
                if (pos < fst_len) {
                    v += (fst[fst_len - 1 - pos] - '0') * (snd[0] - '0');
                }
                res[fst_len - pos] = (v % 10) + '0';
                carry = v / 10;
                pos++;
            } while (pos <= fst_len);
            char* non_zero = res;
            while (*non_zero == '0') {
                non_zero++;
            }
            if (strlen(non_zero) == 0) {
                non_zero--;
            }
            memmove(res, non_zero, strlen(non_zero) + 1);
            return res;
        } else {
            char* res = strdup("0");
            size_t snd_len = strlen(snd);
            for (size_t i = 0; snd[i] != 0; i++) {
                char dig[2] = { snd[i], 0 };
                char* part = decShiftInline(mulInteger(fst, dig), snd_len - 1 - i);
                char* next = addInteger(res, part);
                free(part);
                free(res);
                res = next;
            }
            return res;
        }
    }
}

static void divRemInteger(const char* fst, const char* snd, char** div, char** rem) {
    *div = strdup(fst);
    *rem = strdup("0");
    for (size_t i = 0; fst[i] != 0; i++) {
        *rem = decShiftInline(*rem, 1);
        (*rem)[strlen(*rem) - 1] = fst[i];
        int digit = 0;
        while (cmpInteger(*rem, snd) >= 0) {
            digit++;
            char* next = subInteger(*rem, snd);
            free(*rem);
            *rem = next;
        }
        (*div)[i] = digit + '0';
    }
    char* non_zero = *div;
    while (*non_zero == '0') {
        non_zero++;
    }
    if (strlen(non_zero) == 0) {
        non_zero--;
    }
    memmove(*div, non_zero, strlen(non_zero) + 1);
}

static char* divInteger(const char* fst, const char* snd) {
    if (*fst == '-' && *snd == '-') {
        return divInteger(fst + 1, snd + 1);
    } else if (*fst == '-') {
        return negInline(divInteger(fst + 1, snd));
    } else if (*snd == '-') {
        return negInline(divInteger(fst, snd + 1));
    } else {
        char* div;
        char* rem;
        divRemInteger(fst, snd, &div, &rem);
        free(rem);
        return div;
    }
}

static char* remInteger(const char* fst, const char* snd) {
    if (*fst == '-' && *snd == '-') {
        return negInline(remInteger(fst + 1, snd + 1));
    } else if (*fst == '-') {
        return negInline(remInteger(fst + 1, snd));
    } else if (*snd == '-') {
        return remInteger(fst, snd + 1);
    } else {
        char* div;
        char* rem;
        divRemInteger(fst, snd, &div, &rem);
        free(div);
        return rem;
    }
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
    } else if (eq(arg, "+")) {
        return strdup(next(args));
    } else {
        return strdup(arg);
    }
}

static char* evalPre(Scanner* args) {
    const char* arg = peek(args);
    if (eq(arg, "substr")) {
        next(args);
        char* str = evalBase(args);
        char* pos = evalBase(args);
        char* len = evalBase(args);
        assertInteger(pos);
        assertInteger(len);
        size_t posi, leni;
        sscanf(pos, "%lu", &posi);
        sscanf(len, "%lu", &leni);
        if (posi == 0) {
            leni = 0;
        } else {
            posi--;
        }
        if (posi > strlen(str)) {
            posi = strlen(str);
        }
        if (posi + leni > strlen(str)) {
            leni = strlen(str) - posi;
        }
        char* res = malloc(leni + 1);
        memcpy(res, str + posi, leni);
        res[leni] = 0;
        return res;
    } else if (eq(arg, "index")) {
        next(args);
        char* haystack = evalBase(args);
        char* needle = evalBase(args);
        char* found = strstr(haystack, needle);
        free(haystack);
        free(needle);
        size_t idx = found == NULL ? 0 : found - haystack + 1;
        char* res = malloc(32);
        sprintf(res, "%lu", idx);
        return res;
    } else if (eq(arg, "length")) {
        next(args);
        char* val = evalBase(args);
        size_t len = strlen(val);
        free(val);
        char* res = malloc(32);
        sprintf(res, "%lu", len);
        return res;
    } else {
        return evalBase(args);
    }
}

static char* evalMul(Scanner* args) {
    char* res = evalPre(args);
    while (
        peek(args) != NULL && (eq(peek(args), "*") || eq(peek(args), "/") || eq(peek(args), "%"))
    ) {
        const char* op = next(args);
        char* fst = res;
        char* snd = evalMul(args);
        assertInteger(res);
        assertInteger(snd);
        if (eq(op, "*")) {
            res = mulInteger(fst, snd);
        } else if (eq(op, "/")) {
            res = divInteger(fst, snd);
        } else if (eq(op, "%")) {
            res = remInteger(fst, snd);
        }
        free(fst);
        free(snd);
    }
    return res;
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
            eq(peek(args), "<") || eq(peek(args), "<=") || eq(peek(args), "=")
            || eq(peek(args), "!=") || eq(peek(args), ">=") || eq(peek(args), ">")
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
        fprintf(stderr, "%s: missing operand\n", prog);
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

