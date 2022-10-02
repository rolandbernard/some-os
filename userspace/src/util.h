#ifndef _UTIL_H_
#define _UTIL_H_

char* joinPaths(const char* base, const char* name);

char* dirname(const char* path);

const char* basename(const char* path);

int decimalWidth(long i);

#endif
