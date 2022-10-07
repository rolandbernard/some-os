#ifndef _PATH_H_
#define _PATH_H_

void inlineReducePath(char* path);

char* reducedPathCopy(const char* path);

char* stringClone(const char* path);

char* getParentPath(const char* path);

char* getBaseFilename(const char* path);

#endif
