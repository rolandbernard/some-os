
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

typedef enum {
    FLAG_ALTERNATE = 0b0000001,
    FLAG_ZEROPAD   = 0b0000010,
    FLAG_LEFT      = 0b0000100,
    FLAG_SPACE     = 0b0001000,
    FLAG_PLUS      = 0b0010000,
    FLAG_SIGNED    = 0b0100000,
    FLAG_UPPER     = 0b1000000,
} Flags;

typedef enum {
    LENGTH_CHAR,
    LENGTH_SHORT,
    LENGTH_INT,
    LENGTH_LONG,
    LENGTH_LONG_LONG,
} Length;

static int readInt(const char** str) {
    int n = 0;
    while (**str >= '0' && **str <= '9') {
        n *= 10;
        n += **str - '0';
        (*str)++;
    }
    return n;
}

static void writeChar(char* buf, size_t* pos, size_t max, char c) {
    if (*pos < max) {
        buf[*pos] = c;
    }
    (*pos)++;
}

static const char* upper_digits = "0123456789ABCDEF";
static const char* lower_digits = "0123456789abcdef";

static void formatInteger(char* buf, size_t* pos, size_t max, long long num, int base, int width, Flags flags) {
    unsigned long long n = num;
    char prefix[8];
    int j = 0;
    const char* digits = lower_digits;
    if ((flags & FLAG_UPPER) != 0) {
        digits = upper_digits;
    }
    if (num < 0 && (flags & FLAG_SIGNED) != 0) {
        n = -n;
        prefix[j] = '-';
        j++;
    } else if ((flags & FLAG_PLUS) != 0) {
        prefix[j] = '+';
        j++;
    } else if ((flags & FLAG_SPACE) != 0) {
        prefix[j] = ' ';
        j++;
    }
    if ((flags & FLAG_ALTERNATE) != 0) {
        if (base == 8 || base == 16) {
            prefix[j] = '0';
            j++;
            if (base == 16) {
                if ((flags & FLAG_UPPER) != 0) {
                    prefix[j] = 'X';
                } else {
                    prefix[j] = 'x';
                }
                j++;
            }
        }
    }
    char buffer[64];
    int i = 0;
    do {
        buffer[i] = digits[n % base];
        n /= base;
        i++;
    } while (n != 0);
    int padding = 0;
    if (width > 0) {
        padding = width - i - j;
    }
    if (padding < 0) {
        padding = 0;
    }
    if ((flags & FLAG_LEFT) == 0 && (flags & FLAG_ZEROPAD) == 0) {
        for (; padding > 0; padding--) {
            writeChar(buf, pos, max, ' ');
        }
    }
    for (int k = 0; k < j; k++) {
        writeChar(buf, pos, max, prefix[k]);
    }
    if ((flags & FLAG_LEFT) == 0 && (flags & FLAG_ZEROPAD) != 0) {
        for (; padding > 0; padding--) {
            writeChar(buf, pos, max, '0');
        }
    }
    for (; i > 0; i--) {
        writeChar(buf, pos, max, buffer[i - 1]);
    }
    if ((flags & FLAG_LEFT) != 0) {
        for (; padding > 0; padding--) {
            writeChar(buf, pos, max, ' ');
        }
    }
}

static size_t stringLength(const char* str) {
    int n = 0;
    while (*str != 0) {
        str++;
        n++;
    }
    return n;
}

static void formatString(char* buf, size_t* pos, size_t max, const char* str, int width, Flags flags) {
    int padding = 0;
    if (width > 0) {
        padding = width - stringLength(str);
    }
    if (padding < 0) {
        padding = 0;
    }
    if ((flags & FLAG_LEFT) == 0) {
        for (; padding > 0; padding--) {
            writeChar(buf, pos, max, ' ');
        }
    }
    while (*str) {
        writeChar(buf, pos, max, *str);
        str++;
    }
    if ((flags & FLAG_LEFT) != 0) {
        for (; padding > 0; padding--) {
            writeChar(buf, pos, max, ' ');
        }
    }
}

static void formatCharacter(char* buf, size_t* pos, size_t max, char c, int width, Flags flags) {
    char str[2];
    str[0] = c;
    str[1] = 0;
    formatString(buf, pos, max, str, width, flags);
}

int vsnprintf(char* buf, size_t size, const char* fmt, va_list args) {
    size_t pos = 0;
    if (buf == NULL) {
        size = 0;
    }
    while (*fmt != 0) {
        if (*fmt == '%') {
            fmt++;
            Flags flags = 0;
            int width = 0;
            int precision = -1;
            Length length = LENGTH_INT;
            for (;;) {
                if (*fmt == '#') {
                    flags |= FLAG_ALTERNATE;
                } else if (*fmt == '0') {
                    flags |= FLAG_ZEROPAD;
                } else if (*fmt == '-') {
                    flags |= FLAG_LEFT;
                } else if (*fmt == ' ') {
                    flags |= FLAG_SPACE;
                } else if (*fmt == '+') {
                    flags |= FLAG_PLUS;
                } else {
                    break;
                }
                fmt++;
            }
            if (*fmt == '*') {
                fmt++;
                width = va_arg(args, int);
            } else {
                width = readInt(&fmt);
            }
            if (width < 0) {
                width = -width;
                flags |= FLAG_LEFT;
            }
            if (*fmt == '.') {
                fmt++;
                if (*fmt == '*') {
                    fmt++;
                    precision = va_arg(args, int);
                } else {
                    precision = readInt(&fmt);
                }
                if (precision < 0) {
                    precision = 0;
                }
            }
            if (*fmt == 'h') {
                fmt++;
                if (*fmt == 'h') {
                    fmt++;
                    length = LENGTH_CHAR;
                } else {
                    length = LENGTH_SHORT;
                }
            } else if (*fmt == 'l') {
                if (*fmt == 'l') {
                    fmt++;
                    length = LENGTH_LONG_LONG;
                } else {
                    length = LENGTH_LONG;
                }
            }
            if (*fmt == 'i' || *fmt == 'd' || *fmt == 'o' || *fmt == 'u' || *fmt == 'x' || *fmt == 'X') {
                long long num = 0;
                if (length == LENGTH_LONG_LONG) {
                    num = va_arg(args, long long);
                } else if (length == LENGTH_LONG) {
                    num = va_arg(args, long);
                } else {
                    num = va_arg(args, int);
                    if (length == LENGTH_SHORT) {
                        num &= 0xffff;
                    } else if (length == LENGTH_CHAR) {
                        num &= 0xff;
                    }
                }
                int base = 10;
                if (*fmt == 'i' || *fmt == 'd') {
                    flags |= FLAG_SIGNED;
                } else if (*fmt == 'X') {
                    flags |= FLAG_UPPER;
                    base = 16;
                } else if (*fmt == 'x') {
                    base = 16;
                } else if (*fmt == 'o') {
                    base = 8;
                }
                formatInteger(buf, &pos, size, num, base, width, flags);
            } else if (*fmt == 'p') {
                long long num = (long long)va_arg(args, void*);
                formatInteger(buf, &pos, size, num, 16, width, FLAG_ALTERNATE);
            } else if (*fmt == 'c') {
                char c = va_arg(args, int);
                formatCharacter(buf, &pos, size, c, width, flags);
            } else if (*fmt == 's') {
                const char* str = va_arg(args, char*);
                if (str == NULL) {
                    str = "(null)";
                }
                formatString(buf, &pos, size, str, width, flags);
            } else if (*fmt == '%') {
                writeChar(buf, &pos, size, *fmt);
            } else {
                writeChar(buf, &pos, size, '%');
                writeChar(buf, &pos, size, *fmt);
            }
        } else {
            writeChar(buf, &pos, size, *fmt);
        }
        fmt++;
    }
    if (size != 0) {
        if (pos < size) {
            buf[pos] = 0;
        } else {
            buf[size - 1] = 0;
        }
    }
    return pos;
}

int snprintf(char* buf, size_t size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return ret;
}

int vsprintf(char* buf, const char* fmt, va_list args) {
    return vsnprintf(buf, SIZE_MAX, fmt, args);
}

int sprintf(char* buf, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, SIZE_MAX, fmt, args);
    va_end(args);
    return ret;
}

