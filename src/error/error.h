#ifndef _ERROR_H_
#define _ERROR_H_

#include <stdbool.h>

#define CHECKED(OP, ...) { Error res = OP; if (isError(res)) { { __VA_ARGS__ ; } ; return res; } } 

typedef enum {
    SUCCESS = 0,
    UNKNOWN,

    ALREADY_IN_USE,
    ILLEGAL_ARGUMENTS,
    IO_ERROR,
    NOT_INITIALIZED,
    NO_DATA,
    NO_SUCH_FILE,
    UNSUPPORTED,
    WRONG_FILE_TYPE,
    FORBIDDEN,
    INTERRUPTED,
} ErrorKind;

typedef struct {
    ErrorKind kind;
    const char* details;
} Error;

// Create error struct without a message
Error simpleError(ErrorKind kind);

// Create error struct with a message
Error someError(ErrorKind kind, const char* details);

// Return true if the argument is an error
bool isError(Error error);

// Get the message for the given error kind
const char* getErrorKindMessage(ErrorKind error);

// Get the message for the given error
const char* getErrorMessage(Error error);

#endif
