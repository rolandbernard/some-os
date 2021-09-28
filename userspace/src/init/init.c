
#include "libstart/syscall.h"

uint8_t data[512];

int main(int argc, char* argv[], char* env[]) {
    DirectoryEntry* entry = (DirectoryEntry*)data;
    int fd = syscall_open("/test", FILE_OPEN_DIRECTORY, 0);
    while ((int)syscall_readdir(fd, entry, 512) > 0) {
        syscall_print("Entry: ");
        syscall_print(entry->name);
        syscall_print("\n");
    }
    syscall_print("Hello world!\n");
    syscall_close(fd);
    return 1;
}

