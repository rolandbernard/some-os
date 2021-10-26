#ifndef _PAGE_REF_TABLE_H_
#define _PAGE_REF_TABLE_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// This is an implementation of a hash table from uint64_t to uint64_t to be used for the copy on
// write reference counting implementation

typedef struct {
    size_t count;
    size_t capacity;
    uintptr_t* keys;
    size_t* values;
} PageRefTable;

bool hasOtherReferences(PageRefTable* table, uintptr_t page);

void addReferenceFor(PageRefTable* table, uintptr_t page);

void removeReferenceFor(PageRefTable* table, uintptr_t page);

#endif
