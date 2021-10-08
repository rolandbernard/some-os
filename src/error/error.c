
#include <stddef.h>

#include "error/error.h"

Error simpleError(ErrorKind kind) {
    return someError(kind, NULL);
}

Error someError(ErrorKind kind, const char* details) {
    Error ret = {
        .kind = kind,
        .details = details,
    };
    return ret;
}

bool isError(Error error) {
    return error.kind != SUCCESS;
}

const char* getErrorKindMessage(ErrorKind error) {
    switch (error) {
        case SUCCESS: return "Success";
        case UNKNOWN: return "Unknown";
        case NO_DATA: return "No data";
        case UNSUPPORTED: return "Unsupported";
        case NOT_INITIALIZED: return "Not initialized";
        case ALREADY_IN_USE: return "Already in use";
        case ILLEGAL_ARGUMENTS: return "Illegal arguments";
        case IO_ERROR: return "IO error";
        case NO_SUCH_FILE: return "File not found";
        case WRONG_FILE_TYPE: return "Wrong file type";
        case FORBIDDEN: return "Permission denied";
        case INTERRUPTED: return "Interrupted";
    }
    // Like default but we still get an warning for an incomplete switch
    return "Unknown?";
}

const char* getErrorMessage(Error error) {
    // Return the error message in the error is present, otherwise the message for the kind
    if (error.details != NULL) {
        return error.details;
    } else {
        return getErrorKindMessage(error.kind);
    }
}

