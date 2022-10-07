#ifndef _STRINGLIST_H_
#define _STRINGLIST_H_

#include <stdlib.h>

#define LIST_GET(TYPE, LIST, AT) ((TYPE)(LIST).values[AT])

typedef struct {
    void** values;
    size_t count;
    size_t capacity;
} List;

void initList(List* list);

void deinitList(List* list);

void deinitListAndContents(List* list);

void addToList(List* list, void* value);

void copyStringToList(List* list, const char* string);

#endif
