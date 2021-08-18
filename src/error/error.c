
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
        case NO_DATA: return "No data";
        case UNSUPPORTED: return "Unsupported";
        case NOT_INITIALIZED: return "Not initialized";
        default: return "Unknown";
    }
}

const char* getErrorMessage(Error error) {
    // Return the error message in the error is present, otherwise the message for the kind
    if (error.details != NULL) {
        return error.details;
    } else {
        return getErrorKindMessage(error.kind);
    }
}

