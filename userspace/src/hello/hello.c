
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Simple hello world program

int main() {
    fprintf(stderr, "Started hello program\n");
    mkfifo("/fifo", S_IRWXU | S_IRWXG | S_IRWXO);
    int fd = open("/fifo", O_RDWR);
    if (fd == -1) {
        fprintf(stderr, "Error opening fifo: %s\n", strerror(errno));
    } else {
        char buffer[512];
        buffer[read(fd, buffer, 512)] = 0;
        close(fd);
        printf("Hello world! fd: %i\n%s\n", fd, buffer);
    }
    return 42;
}

