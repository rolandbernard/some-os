
#include <string.h>

#include "stringlist.h"

void initStringList(StringList* list) {
    list->strings = NULL;
    list->capacity = 0;
    list->count = 0;
}

void deinitStringList(StringList* list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->strings[i]);
    }
    free(list->strings);
    initStringList(list);
}

void addStringToList(StringList* list, char* string) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 16 : list->capacity * 3 / 2;
        list->strings = (char**)realloc(list->strings, list->capacity * sizeof(char*));
    }
    list->strings[list->count] = string;
    list->count++;
}

void copyStringToList(StringList* list, const char* string) {
    addStringToList(list, strdup(string));
}

