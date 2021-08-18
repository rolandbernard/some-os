#ifndef _ERROR_H_
#define _ERROR_H_

#include <stdbool.h>

#define CHECKED(OP) { Error res = OP; if (isError(res)) { return res; } } 

typedef enum {
    SUCCESS = 0,

    NO_DATA,
    UNSUPPORTED,
    NOT_INITIALIZED,
} ErrorKind;

typedef struct {
    ErrorKind kind;
    const char* details;
} Error;

Error simpleError(ErrorKind kind);

Error someError(ErrorKind kind, const char* details);

bool isError(Error error);

const char* getErrorKindMessage(ErrorKind error);

const char* getErrorMessage(Error error);

#endif
