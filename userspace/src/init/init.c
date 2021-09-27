
#include "libstart/syscall.h"

int main(int argc, char* argv[], char* env[]) {
    syscall_print("Hello world!\n");
    return 1;
}

