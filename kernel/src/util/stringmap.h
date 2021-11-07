#ifndef _STRING_MAP_H_
#define _STRING_MAP_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// This is an implementation of a hash table from strings (char*) to void*

#define STRING_MAP_INITIALIZER { .count = 0, .capacity = 0, .keys = NULL, .values = NULL }

typedef struct {
    size_t count;
    size_t capacity;
    char** keys;
    void** values;
} StringMap;

void putToStringMap(StringMap* map, const char* key, void* value);

void* getFromStringMap(StringMap* map, const char* key);

const char* getKeyFromStringMap(StringMap* map, const char* key);

void deleteFromStringMap(StringMap* map, const char* key);

#endif
