
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

char* joinPaths(const char* base, const char* name) {
    size_t base_len = strlen(base);
    size_t name_len = strlen(name);
    char* result = (char*)malloc(base_len + 1 + name_len);
    size_t insert = 0;
    for (size_t i = 0; i < base_len + 1 + name_len; i++) {
        char next = i < base_len ? base[i] : (i == base_len ? '/' : name[i - base_len - 1]);
        if (insert == 0 || result[insert - 1] != '/' || next != '/') {
            result[insert] = next;
            insert++;
        }
    }
    if (insert > 1 && result[insert - 1] == '/') {
        insert--;
    }
    result[insert] = 0;
    return result;
}

const char* basename(const char* path) {
    size_t path_len = strlen(path);
    while (path_len != 0 && path[path_len - 1] != '/') {
        path_len--;
    }
    return path + path_len;
}

int decimalWidth(long i) {
    int result = 1;
    while (i >= 10) {
        result++;
        i /= 10;
    }
    return result;
}

