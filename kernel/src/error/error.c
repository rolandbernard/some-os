
#include <stddef.h>

#include "error/error.h"

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
        case SUCCESS:   return "Success";
        case EPERM:     return "Operation not permitted";
        case ENOENT:    return "No such file or directory";
        case ESRCH:     return "No such process";
        case EINTR:     return "Interrupted system call";
        case EIO:       return "I/O error";
        case ENXIO:     return "No such device or address";
        case E2BIG:     return "Argument list too long";
        case ENOEXEC:   return "Exec format error";
        case EBADF:     return "Bad file number";
        case ECHILD:    return "No child processes";
        case EAGAIN:    return "Try again";
        case ENOMEM:    return "Out of memory";
        case EACCES:    return "Permission denied";
        case EFAULT:    return "Bad address";
        case ENOTBLK:   return "Block device required";
        case EBUSY:     return "Device or resource busy";
        case EEXIST:    return "File exists";
        case EXDEV:     return "Cross-device link";
        case ENODEV:    return "No such device";
        case ENOTDIR:   return "Not a directory";
        case EISDIR:    return "Is a directory";
        case EINVAL:    return "Invalid argument";
        case ENFILE:    return "File table overflow";
        case EMFILE:    return "Too many open files";
        case ENOTTY:    return "Not a typewriter";
        case ETXTBSY:   return "Text file busy";
        case EFBIG:     return "File too large";
        case ENOSPC:    return "No space left on device";
        case ESPIPE:    return "Illegal seek";
        case EROFS:     return "Read-only file system";
        case EMLINK:    return "Too many links";
        case EPIPE:     return "Broken pipe";
        case EDOM:      return "Math argument out of domain of func";
        case ERANGE:    return "Math result not representable";
        case EUNSUP:    return "Operation not supported";
        case SUCCESS_EXIT: return "Success (exit)";
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

