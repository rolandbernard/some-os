
#include <stdbool.h>

#include "libstart/syscall.h"

uint8_t data[512];

void itoa(char* dest, long num) {
    size_t idx = 0;
    bool neg = false;;
    if (num < 0) {
        num = -num;
        neg = true;
    }
    while (num != 0) {
        dest[idx++] = '0' + (num % 10);
        num /= 10;
    }
    if (neg) {
        dest[idx++] = '-';
    }
    for (size_t i = 0; 2 * i < idx; i++) {
        char tmp = dest[i];
        dest[i] = dest[idx - i - 1];
        dest[idx - i - 1] = tmp;
    }
    dest[idx] = 0;
}

int main(int argc, char* argv[], char* env[]) {
    DirectoryEntry* entry = (DirectoryEntry*)data;
    int pid = syscall_fork();
    if (pid == 0) {
        syscall_print("CHILD\n");
    } else {
        syscall_print("PARENT of ");
        itoa((char*)data, pid);
        syscall_print((char*)data);
        syscall_print("\n");
        int fd = syscall_open("/test", FILE_OPEN_DIRECTORY, 0);
        while ((int)syscall_readdir(fd, entry, 512) > 0) {
            syscall_print("Entry: ");
            syscall_print(entry->name);
            syscall_print("\n");
        }
        syscall_print("Hello world!\n");
        syscall_close(fd);
    }
    return 1;
}

