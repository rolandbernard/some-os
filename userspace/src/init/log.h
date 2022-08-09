#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>
#include <string.h>
#include <errno.h>

#define PROGRAM_NAME "init"

#define STYLE_ERROR "\e[1;91m"
#define STYLE_WARNING "\e[1;93m"
#define STYLE_SUCCESS "\e[92m"
#define STYLE_SUB_SUCCESS "\e[2;92m"
#define STYLE_DEBUG "\e[90m"
#define STYLE_DEBUG_LOC "\e[3;90m"

#define STRING(X) #X
#define STRINGX(X) STRING(X)

#ifdef DEBUG
#define USPACE_LOG2(FMT, ...) \
    fprintf(stderr, FMT "\e[m\n" STYLE_DEBUG_LOC " ∟<%s>\t" __FILE__ ":" STRINGX(__LINE__) "\e[m\n" __VA_OPT__(,) __VA_ARGS__, __PRETTY_FUNCTION__);

#define USPACE_INTERNAL_LOG(FMT, ...)                                           \
    if (errno != 0) {                                                           \
        USPACE_LOG2(FMT ": %s" __VA_OPT__(,) __VA_ARGS__, strerror(errno));     \
        errno = 0;                                                              \
    } else {                                                                    \
        USPACE_LOG2(FMT __VA_OPT__(,) __VA_ARGS__);                             \
    }
#else
#define USPACE_INTERNAL_LOG(FMR, ...) \
    fprintf(stderr, FMT "\n" __VA_OPT__(,) __VA_ARGS__);
#endif

#define USPACE_ERROR(FMT, ...) USPACE_INTERNAL_LOG(STYLE_ERROR "[" PROGRAM_NAME "] " FMT __VA_OPT__(,) __VA_ARGS__)
#define USPACE_WARNING(FMT, ...) USPACE_INTERNAL_LOG(STYLE_WARNING "[" PROGRAM_NAME "] " FMT __VA_OPT__(,) __VA_ARGS__)
#define USPACE_SUCCESS(FMT, ...) USPACE_INTERNAL_LOG(STYLE_SUCCESS "[" PROGRAM_NAME "] " FMT __VA_OPT__(,) __VA_ARGS__)
#define USPACE_SUBSUCCESS(FMT, ...) USPACE_INTERNAL_LOG(STYLE_SUB_SUCCESS "[" PROGRAM_NAME "] " FMT __VA_OPT__(,) __VA_ARGS__)
#define USPACE_DEBUG(FMT, ...) USPACE_INTERNAL_LOG(STYLE_DEBUG "[" PROGRAM_NAME "] " FMT __VA_OPT__(,) __VA_ARGS__)
#define USPACE_DEBUG_LOC(FMT, ...) USPACE_INTERNAL_LOG(STYLE_DEBUG_LOC "[" PROGRAM_NAME "] " FMT __VA_OPT__(,) __VA_ARGS__)

#endif
