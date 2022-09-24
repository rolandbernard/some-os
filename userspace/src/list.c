
#include <string.h>

#include "list.h"

void initList(List* list) {
    list->values = NULL;
    list->capacity = 0;
    list->count = 0;
}

void deinitList(List* list) {
    free(list->values);
    initList(list);
}

void deinitListAndContents(List* list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->values[i]);
    }
    deinitList(list);
}

void addToList(List* list, void* value) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 16 : list->capacity * 3 / 2;
        list->values = (void**)realloc(list->values, list->capacity * sizeof(void*));
    }
    list->values[list->count] = value;
    list->count++;
}

void copyStringToList(List* list, const char* string) {
    addToList(list, strdup(string));
}

