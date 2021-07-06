#ifndef _ERROR_H_
#define _ERROR_H_

typedef enum {
    NONE = 0,
    UNKNOWN,

    UNSUPPORTED,
    INIT_FAILED,
    UNEXPECTED_STATE,
    NO_RESPONSE,
} Error;

#endif
