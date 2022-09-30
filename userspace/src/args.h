#ifndef _ARGS_H_
#define _ARGS_H_

#include <stdbool.h>
#include <string.h>

#define ARG_WARN_UNKNOWN_VALUE() {                      \
    const char* msg_base = "unknown value";             \
    int msg_len = strlen(msg_base);                     \
    int val_len = strlen(value);                        \
    char msg[val_len + msg_len + 3];                    \
    memcpy(msg, value, val_len);                        \
    memcpy(msg + val_len, ": ", 2);                     \
    memcpy(msg + val_len + 2, msg_base, msg_len + 1);   \
    ARG_WARN(msg);                                      \
}

#define ARG_WARN_MULTIPLE() \
    ARG_WARN("multiple values for this option");

#define ARG_WARN_CONFLICT(NAME) \
    ARG_WARN("option conflicts with " NAME);

#define ARG_WARN(ABOUT)                               \
    _raiseWarning(option, ABOUT, argc, argv, context);

#define ARG_SPEC_FUNCTION(NAME, ARG, USAGE, BODY, DEFAULT, WARNING, ...)                            \
    static int NAME(bool _help, int argc, const char* const* argv, ARG context);                    \
    static void _warning_ ## NAME(                                                                  \
        const char* option, const char* warning, int argc, const char* const* argv, ARG context     \
    ) { WARNING }                                                                                   \
    static int NAME(bool _help, int argc, const char* const* argv, ARG context) {                   \
        void (*_raiseWarning)(                                                                      \
            const char*, const char*, int, const char* const*, ARG                                  \
        ) = _warning_ ## NAME;                                                                      \
        int _args = 0;                                                                              \
        int _len = 0;                                                                               \
        int _i = 0, _j = 0;                                                                         \
        bool only_default = false;                                                                  \
        if (_help) {                                                                                \
            fputs("Usage: " USAGE "\n", stderr);                                                    \
            fputs("Options:\n", stderr);                                                            \
            do {                                                                                    \
                bool _letters = false;                                                              \
                bool _names = false;                                                                \
                const char* option = NULL;                                                          \
                BODY                                                                                \
            } while (false);                                                                        \
        } else {                                                                                    \
            for (_i = 1; _i < argc; _i++) {                                                         \
                if (!only_default && argv[_i][0] == '-' && argv[_i][1] != 0) {                      \
                    if (argv[_i][1] == '-') {                                                       \
                        if (argv[_i][2] == 0) {                                                     \
                            only_default = true;                                                    \
                        } else {                                                                    \
                            bool _letters = false;                                                  \
                            bool _names = true;                                                     \
                            int _len = 0;                                                           \
                            while (argv[_i][_len + 2] != 0 && argv[_i][_len + 2] != '=') {          \
                                _len++;                                                             \
                            }                                                                       \
                            char option[_len + 3];                                                  \
                            memcpy(option, argv[_i], _len + 2);                                     \
                            option[_len + 2] = 0;                                                   \
                            _args++;                                                                \
                            BODY                                                                    \
                            ARG_WARN("unknown command line option");                                \
                            _args--;                                                                \
                        }                                                                           \
                    } else {                                                                        \
                        bool _letters = true;                                                       \
                        bool _names = false;                                                        \
                        for (_j = 1; argv[_i][_j] != 0; _j++) {                                     \
                            const char option[3] = {'-', argv[_i][_j], 0};                          \
                            _args++;                                                                \
                            BODY                                                                    \
                            ARG_WARN("unknown command line option");                                \
                            _args--;                                                                \
                        }                                                                           \
                    }                                                                               \
                } else {                                                                            \
                    const char* value = argv[_i];                                                   \
                    _args++;                                                                        \
                    DEFAULT                                                                         \
                }                                                                                   \
            }                                                                                       \
            __VA_ARGS__                                                                             \
        }                                                                                           \
        return _args;                                                                               \
    }

#define ARG_PRINT_HELP(SPEC, ARG) SPEC(true, 0, NULL, ARG)

#define ARG_PARSE_ARGS(SPEC, ARGC, ARGV, ARG) SPEC(false, ARGC, ARGV, ARG)

#define ARG_FLAG(LETTER, NAME, ACTION, DESC)                                    \
    if (_help) {                                                                \
        printOptionHelpLine(LETTER, NAME, NULL, DESC);                          \
    } else if (_letters) {                                                      \
        if (LETTER != 0 && argv[_i][_j] == LETTER) {                            \
            ACTION; continue;                                                   \
        }                                                                       \
    } else if (_names) {                                                        \
        const char* _name = NAME;                                               \
        if (_name != NULL && strcmp(option + 2, _name) == 0) {                  \
            if (argv[_i][_len + 2] == '=') {                                    \
                ARG_WARN("option does not expect a value");                     \
            }                                                                   \
            ACTION; continue;                                                   \
        }                                                                       \
    }

#define ARG_VALUED_BASE(LETTER, NAME, ACTION, TAGGED, FOLLOWED, VALUE_DESC, DESC)   \
    if (_help) {                                                                    \
        printOptionHelpLine(LETTER, NAME, VALUE_DESC, DESC);                        \
    } else if (_letters) {                                                          \
        if (LETTER != 0 && argv[_i][_j] == LETTER) {                                \
            if (argv[_i][_j + 1] == '=') {                                          \
                const char* value = argv[_i] + _j + 2;                              \
                _j = strlen(argv[_i]) - 1;                                          \
                ACTION; continue;                                                   \
            } else {                                                                \
                const char* value = NULL;                                           \
                if (_j == 1) {                                                      \
                    if (argv[_i][_j + 1] != 0) {                                    \
                        value = argv[_i] + _j + 1;                                  \
                        _j = strlen(argv[_i]) - 1;                                  \
                    } else if (_i + 1 < argc && argv[_i + 1][0] != '-') {           \
                        value = argv[_i + 1];                                       \
                        _i++;                                                       \
                        _j = strlen(argv[_i]) - 1;                                  \
                    }                                                               \
                }                                                                   \
                ACTION; continue;                                                   \
            }                                                                       \
        }                                                                           \
    } else if (_names) {                                                            \
        const char* name = NAME;                                                    \
        if (name != NULL) {                                                         \
            if(strcmp(option + 2, name) == 0) {                                     \
                if (argv[_i][_len + 2] == '=') {                                    \
                    const char* value = argv[_i] + _len + 3;                        \
                    ACTION; continue;                                               \
                } else {                                                            \
                    const char* value = NULL;                                       \
                    if (FOLLOWED && _i + 1 < argc && argv[_i + 1][0] != '-') {      \
                        value = argv[_i + 1];                                       \
                        _i++;                                                       \
                    }                                                               \
                    ACTION; continue;                                               \
                }                                                                   \
            }                                                                       \
        }                                                                           \
    }

#define ARG_VALUED(LETTER, NAME, ACTION, OPTIONAL, VALUE_DESC, DESC)    \
    ARG_VALUED_BASE(LETTER, NAME, {                                     \
        if (!OPTIONAL && value == NULL) {                               \
            ARG_WARN("expected a value");                               \
        } else {                                                        \
            ACTION                                                      \
        }                                                               \
    }, true, !OPTIONAL, VALUE_DESC, DESC)

#define ARG_STRING_LIST(LETTER, NAME, ACTION, VALUE_DESC, DESC)         \
    ARG_VALUED(LETTER, NAME, {                                          \
        if (value != NULL) {                                            \
            while (value[0] != 0) {                                     \
                int len = 0;                                            \
                while (value[len] != 0 && value[len] != ',') {          \
                    len++;                                              \
                }                                                       \
                {                                                       \
                    char next_value[len + 1];                           \
                    memcpy(next_value, value, len);                     \
                    next_value[len] = 0;                                \
                    char* value = next_value;                           \
                    ACTION                                              \
                }                                                       \
                if (value[len] == ',') {                                \
                    value += len + 1;                                   \
                } else {                                                \
                    value += len;                                       \
                }                                                       \
            }                                                           \
        }                                                               \
    }, false, VALUE_DESC, DESC);

#define ARG_INTEGER(LETTER, NAME, ACTION, VALUE_DESC, DESC)             \
    ARG_VALUED(LETTER, NAME, {                                          \
        bool is_integer = true;                                         \
        int int_value = 0;                                              \
        for (int i = 0; value[i] != 0; i++) {                           \
            if (value[i] >= '0' && value[i] <= '9') {                   \
                int_value *= 10;                                        \
                int_value += value[i] - '0';                            \
            } else {                                                    \
                is_integer = false;                                     \
                break;                                                  \
            }                                                           \
        }                                                               \
        if (!is_integer) {                                              \
            ARG_WARN("expected an integer value");                      \
        } else {                                                        \
            int value = int_value;                                      \
            ACTION                                                      \
        }                                                               \
    }, false, VALUE_DESC, DESC)

void printOptionHelpLine(char single, const char* word, const char* value, const char* desc);

#endif
