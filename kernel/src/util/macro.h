#ifndef _MACRO_H_
#define _MACRO_H_

#define STRING(X) #X
#define STRINGX(X) STRING(X)

#define SOMETHING 1
#define NOTHING

#define CONSOME(X)
#define KEEP(X) X

#define FIRST(X, ...) X
#define REST(X, ...) __VA_ARGS__

#define IS_EMPTY(...) IFNE(__VA_ARGS__)(1, 0)

#define IFN(...) CONSOME __VA_OPT__(()KEEP)

#define IFE(...) KEEP __VA_OPT__(()CONSOME)

#define IFNE(...) REST __VA_OPT__(()FIRST)

#endif
