
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

// Simple hello world program

int main() {
    fprintf(stderr, "Started hello program\n");
    mkfifo("/fifo", S_IRWXU | S_IRWXG | S_IRWXO);
    int fd = open("/fifo", O_RDWR);
    char buffer[512];
    buffer[read(fd, buffer, 512)] = 0;
    close(fd);
    printf("Hello world! fd: %i\n%s\n", fd, buffer);
    return 42;
}

