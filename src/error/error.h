#ifndef _ERROR_H_
#define _ERROR_H_

typedef enum {
    SUCCESS = 0,

    UNSUPPORTED,
    INIT_FAILED,
    UNEXPECTED_STATE,
    NO_RESPONSE,
} Error;

#endif
