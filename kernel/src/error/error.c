
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
        case EPERM:     return "Not owner";
        case ENOENT:    return "No such file or directory";
        case ESRCH:     return "No such process";
        case EINTR:     return "Interrupted system call";
        case EIO:       return "I/O error";
        case ENXIO:     return "No such device or address";
        case E2BIG:     return "Arg list too long";
        case ENOEXEC:   return "Exec format error";
        case EBADF:     return "Bad file number";
        case ECHILD:    return "No children";
        case EAGAIN:    return "No more processes";
        case ENOMEM:    return "Not enough space";
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
        case ENFILE:    return "Too many open files in system";
        case EMFILE:    return "File descriptor value too large";
        case ENOTTY:    return "Not a character device";
        case ETXTBSY:   return "Text file busy";
        case EFBIG:     return "File too large";
        case ENOSPC:    return "No space left on device";
        case ESPIPE:    return "Illegal seek";
        case EROFS:     return "Read-only file system";
        case EMLINK:    return "Too many links";
        case EPIPE:     return "Broken pipe";
        case EDOM:      return "Mathematics argument out of domain of function";
        case ERANGE:    return "Result too large";
        case ENOMSG:    return "No message of desired type";
        case EIDRM:     return "Identifier removed";
        case ECHRNG:    return "Channel number out of range";
        case EL2NSYNC:  return "Level 2 not synchronized";
        case EL3HLT:    return "Level 3 halted";
        case EL3RST:    return "Level 3 reset";
        case ELNRNG:    return "Link number out of range";
        case EUNATCH:   return "Protocol driver not attached";
        case ENOCSI:    return "No CSI structure available";
        case EL2HLT:    return "Level 2 halted";
        case EDEADLK:   return "Deadlock";
        case ENOLCK:    return "No lock";
        case EBADE:     return "Invalid exchange";
        case EBADR:     return "Invalid request descriptor";
        case EXFULL:    return "Exchange full";
        case ENOANO:    return "No anode";
        case EBADRQC:   return "Invalid request code";
        case EBADSLT:   return "Invalid slot";
        case EDEADLOCK: return "File locking deadlock error";
        case EBFONT:    return "Bad font file fmt";
        case ENOSTR:    return "Not a stream";
        case ENODATA:   return "No data (for no delay io)";
        case ETIME:     return "Stream ioctl timeout";
        case ENOSR:     return "No stream resources";
        case ENONET:    return "Machine is not on the network";
        case ENOPKG:    return "Package not installed";
        case EREMOTE:   return "The object is remote";
        case ENOLINK:   return "Virtual circuit is gone";
        case EADV:      return "Advertise error";
        case ESRMNT:    return "Srmount error";
        case ECOMM:     return "Communication error on send";
        case EPROTO:    return "Protocol error";
        case EMULTIHOP: return "Multihop attempted";
        case ELBIN:     return "Inode is remote (not really error)";
        case EDOTDOT:   return "Cross mount point (not really error)";
        case EBADMSG:   return "Bad message";
        case EFTYPE:    return "Inappropriate file type or format";
        case ENOTUNIQ:  return "Given log. name not unique";
        case EBADFD:    return "f.d. invalid for this operation";
        case EREMCHG:   return "Remote address changed";
        case ELIBACC:   return "Can't access a needed shared lib";
        case ELIBBAD:   return "Accessing a corrupted shared lib";
        case ELIBSCN:   return ".lib section in a.out corrupted";
        case ELIBMAX:   return "Attempting to link in too many libs";
        case ELIBEXEC:  return "Attempting to exec a shared library";
        case ENOSYS:    return "Function not implemented";
        case ENMFILE:       return "No more files";
        case ENOTEMPTY:     return "Directory not empty";
        case ENAMETOOLONG:  return "File or path name too long";
        case ELOOP:         return "Too many symbolic links";
        case EOPNOTSUPP:    return "Operation not supported on socket";
        case EPFNOSUPPORT:  return "Protocol family not supported";
        case ECONNRESET:    return "Connection reset by peer";
        case ENOBUFS:       return "No buffer space available";
        case EAFNOSUPPORT:  return "Address family not supported by protocol family";
        case EPROTOTYPE:    return "Protocol wrong type for socket";
        case ENOTSOCK:      return "Socket operation on non-socket";
        case ENOPROTOOPT:   return "Protocol not available";
        case ESHUTDOWN:     return "Can't send after socket shutdown";
        case ECONNREFUSED:  return "Connection refused";
        case EADDRINUSE:    return "Address already in use";
        case ECONNABORTED:  return "Software caused connection abort";
        case ENETUNREACH:   return "Network is unreachable";
        case ENETDOWN:      return "Network interface is not configured";
        case ETIMEDOUT:     return "Connection timed out";
        case EHOSTDOWN:     return "Host is down";
        case EHOSTUNREACH:  return "Host is unreachable";
        case EINPROGRESS:   return "Connection already in progress";
        case EALREADY:      return "Socket already connected";
        case EDESTADDRREQ:  return "Destination address required";
        case EMSGSIZE:          return "Message too long";
        case EPROTONOSUPPORT:   return "Unknown protocol";
        case ESOCKTNOSUPPORT:   return "Socket type not supported";
        case EADDRNOTAVAIL:     return "Address not available";
        case ENETRESET:         return "Connection aborted by network";
        case EISCONN:           return "Socket is already connected";
        case ENOTCONN:          return "Socket is not connected";
        case ETOOMANYREFS:      return "Too many refs";
        case EPROCLIM:          return "Proc limit";
        case EUSERS:            return "Users";
        case EDQUOT:            return "Disk quota";
        case ESTALE:            return "Stale";
        case ENOTSUP:           return "Not supported";
        case ENOMEDIUM:         return "No medium (in tape drive)";
        case ENOSHARE:          return "No such host or network path";
        case ECASECLASH:        return "Filename exists with different case";
        case EILSEQ:            return "Illegal byte sequence";
        case EOVERFLOW:         return "Value too large for defined data type";
        case ECANCELED:         return "Operation canceled";
        case ENOTRECOVERABLE:   return "State not recoverable";
        case EOWNERDEAD:        return "Previous owner died";
        case ESTRPIPE:          return "Streams pipe error";
        case SUCCESS_EXIT:      return "Success (exit)";
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

