#ifndef _SERIAL_TTYCTL_H_
#define _SERIAL_TTYCTL_H_

// Constants and types for tty ioctl syscalls. Not all of these are used right now.

#define TCGETS      0x5401
#define TCSETS      0x5402
#define TCSETSW     0x5403
#define TCSETSF     0x5404
#define TIOCGPGRP   0x540F
#define TIOCSPGRP   0x5410
#define TCXONC      0x540A
#define TCFLSH      0x540B
#define FIONREAD    0x541B

#define IGNBRK 000001
#define BRKINT 000002
#define IGNPAR 000004
#define INPCK 000020
#define ISTRIP 000040
#define INLCR 000100
#define IGNCR 000200
#define ICRNL 000400
#define IXON 002000
#define IXOFF 010000

#define OPOST 000001
#define OCRNL 000004
#define ONLCR 000010
#define ONOCR 000020
#define TAB3 014000

#define CLOCAL 004000
#define CREAD 000200
#define CSIZE 000060
#define CS5 0
#define CS6 020
#define CS7 040
#define CS8 060
#define CSTOPB 000100
#define HUPCL 002000
#define PARENB 000400
#define PAODD 001000

#define ISIG 0000001
#define ICANON 0000002
#define ECHO 0000010
#define ECHOE 0000020
#define ECHOK 0000040
#define ECHONL 0000100
#define NOFLSH 0000200
#define IEXTEN 0000400
#define TOSTOP 0001000

#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5
#define VMIN 6
#define VSWTC 7
#define VSTART 8
#define VSTOP 9
#define VSUSP 10
#define VEOL 11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE 14
#define VLNEXT 15
#define VEOL2 16

#define B0 000000
#define B50 000001
#define B75 000002
#define B110 000003
#define B134 000004
#define B150 000005
#define B200 000006
#define B300 000007
#define B600 000010
#define B1200 000011
#define B1800 000012
#define B2400 000013
#define B4800 000014
#define B9600 000015
#define B19200 000016
#define B38400 000017

typedef unsigned char TcCc;
typedef unsigned short TcFlags;
typedef char TcSpeed;

typedef struct {
    TcFlags iflag;  /* input mode flags */
    TcFlags oflag;  /* output mode flags */
    TcFlags cflag;  /* control mode flags */
    TcFlags lflag;  /* local mode flags */
    TcCc line;      /* line discipline */
    TcCc cc[32];    /* control characters */
    TcSpeed ispeed; /* input speed */
    TcSpeed ospeed; /* output speed */
} Termios;

#endif
