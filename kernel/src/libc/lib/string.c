
#include <stdbool.h>
#include <stddef.h>

int strcmp(const char* s1, const char* s2) {
    while (*s1 != 0 && s2 != 0) {
        if (*s1 != *s2) {
            return *s1 - *s2;
        }
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n > 0 && *s1 != 0 && s2 != 0) {
        if (*s1 != *s2) {
            return *s1 - *s2;
        }
        s1++;
        s2++;
        n--;
    }
    if (n > 0) {
        return *s1 - *s2;
    } else {
        return 0;
    }
}

size_t strlen(const char* s) {
    size_t length = 0;
    while (s[length] != 0) {
        length++;
    }
    return length;
}

char* strstr(const char* s1, const char* s2) {
    while (*s1 != 0) {
        const char* st1 = s1;
        const char* st2 = s2;
        while (*st1 != 0 && *st2 != 0) {
            if (*st1 != *st2) {
                break;
            }
            st1++;
            st2++;
        }
        if (*st2 == 0) {
            return (char*)s1;
        }
        s1++;
    }
    return NULL;
}

