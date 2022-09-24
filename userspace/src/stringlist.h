#ifndef _STRINGLIST_H_
#define _STRINGLIST_H_

#include <stdlib.h>

typedef struct {
    char** strings;
    size_t count;
    size_t capacity;
} StringList;

void initStringList(StringList* list);

void deinitStringList(StringList* list);

void addStringToList(StringList* list, char* string);

void copyStringToList(StringList* list, const char* string);

#endif
