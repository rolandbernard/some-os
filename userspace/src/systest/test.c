
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define PROGRAM_NAME "test"
#include "log.h"

#define ASSERT(COND)                                \
    if (!(COND)) {                                  \
        USPACE_ERROR("Failed assertion: " #COND);   \
        return false;                               \
    }                                               \
    errno = 0;

#define ASSERT_CHILD(COND)                          \
    if (!(COND)) {                                  \
        USPACE_ERROR("Failed assertion: " #COND);   \
        exit(1);                                    \
    }

static inline intptr_t syscall0(uintptr_t _kind) {
    register uintptr_t kind asm("a0") = _kind;
    register uintptr_t result asm("a0");
    asm volatile(
        "ecall;"
        : "=r" (result)
        : "0" (kind)
        : "memory"
    );
    return result;
}

static bool testSyscallYield() {
    // Test that it does not crash.
    syscall0(2);
    return true;
}

static bool testForkExitWait() {
    int pid = fork();
    int test = 42;
    ASSERT(pid != -1);
    if (pid == 0) {
        test = 555;
        exit(42);
    } else {
        int status;
        int wait_pid = wait(&status);
        ASSERT(wait_pid == pid);
        ASSERT(WIFEXITED(status));
        ASSERT(!WIFSIGNALED(status));
        ASSERT(WEXITSTATUS(status) == 42);
        ASSERT(test == 42);
    }
    return true;
}

static bool testForkWaitNohang() {
    int pid = fork();
    ASSERT(pid != -1);
    if (pid == 0) {
        usleep(1000);
        exit(0);
    } else {
        int status;
        int wait_pid = waitpid(-1, &status, WNOHANG);
        ASSERT(wait_pid == -1);
        wait_pid = wait(&status);
        ASSERT(wait_pid == pid);
        ASSERT(WIFEXITED(status));
        ASSERT(!WIFSIGNALED(status));
        ASSERT(WEXITSTATUS(status) == 0);
    }
    return true;
}

static bool testSigStopCont() {
    int pid = fork();
    ASSERT(pid != -1);
    if (pid == 0) {
        usleep(1000);
        exit(0);
    } else {
        kill(pid, SIGSTOP);
        usleep(10000);
        int status;
        int wait_pid = waitpid(-1, &status, WNOHANG);
        ASSERT(wait_pid == -1);
        kill(pid, SIGCONT);
        wait_pid = wait(&status);
        ASSERT(wait_pid == pid);
        ASSERT(WIFEXITED(status));
        ASSERT(!WIFSIGNALED(status));
        ASSERT(WEXITSTATUS(status) == 0);
    }
    return true;
}

static bool testForkExecWait() {
    int pid = fork();
    ASSERT(pid != -1);
    if (pid == 0) {
        execl("/bin/hello", "/bin/hello", NULL);
        exit(1);
    } else {
        int status;
        int wait_pid = wait(&status);
        ASSERT(wait_pid == pid);
        ASSERT(WIFEXITED(status));
        ASSERT(!WIFSIGNALED(status));
        ASSERT(WEXITSTATUS(status) == 0);
    }
    return true;
}

static bool testClock() {
    clock_t t = clock();
    ASSERT(clock() >= t);
    ASSERT(clock() > t);
    return true;
}

static bool testUsleep() {
    clock_t t1 = times(NULL);
    ASSERT(usleep(10000) == 0);
    clock_t t2 = times(NULL);
    ASSERT(t2 >= t1 + CLOCKS_PER_SEC / 100);
    ASSERT(t2 <= t1 + CLOCKS_PER_SEC / 10);
    return true;
}

static bool testNanosleep() {
    clock_t t1 = times(NULL);
    struct timespec t = {
        .tv_sec = 0, .tv_nsec = 10000000
    };
    ASSERT(nanosleep(&t, &t) == 0)
    clock_t t2 = times(NULL);
    ASSERT(t2 >= t1 + CLOCKS_PER_SEC / 100);
    ASSERT(t2 <= t1 + CLOCKS_PER_SEC / 10);
    return true;
}

static bool testForkKillWait() {
    int pid = fork();
    ASSERT(pid != -1);
    if (pid == 0) {
        while (true) {
            syscall0(2);
        }
        exit(1);
    } else {
        ASSERT(kill(pid, SIGKILL) == 0);
        int status;
        int wait_pid = wait(&status);
        ASSERT(wait_pid == pid);
        ASSERT(!WIFEXITED(status));
        ASSERT(WIFSIGNALED(status));
        ASSERT(WTERMSIG(status) == SIGKILL);
    }
    return true;
}

static bool testForkSleepKillWait() {
    int pid = fork();
    ASSERT(pid != -1);
    if (pid == 0) {
        sleep(1);
        exit(1);
    } else {
        usleep(10000);
        ASSERT(kill(pid, SIGKILL) == 0);
        int status;
        int wait_pid = wait(&status);
        ASSERT(wait_pid == pid);
        ASSERT(!WIFEXITED(status));
        ASSERT(WIFSIGNALED(status));
        ASSERT(WTERMSIG(status) == SIGKILL);
    }
    return true;
}

static void testSignalHandler(int sig) {
    exit(sig == SIGUSR2 ? 42 : 0);
}

static bool testSignal() {
    int pid = fork();
    ASSERT(pid != -1);
    if (pid == 0) {
        signal(SIGUSR2, testSignalHandler);
        sleep(1);
        exit(1);
    } else {
        usleep(10000);
        ASSERT(kill(pid, SIGUSR2) == 0);
        int status;
        int wait_pid = wait(&status);
        ASSERT(wait_pid == pid);
        ASSERT(WIFEXITED(status));
        ASSERT(!WIFSIGNALED(status));
        ASSERT(WEXITSTATUS(status) == 42);
    }
    return true;
}

static bool testGetpid() {
    // Note: This is run as the first child of init.
    ASSERT(getpid() == 2);
    return true;
}

static bool testGetppid() {
    // Note: This is run as a child of init.
    ASSERT(getppid() == 1);
    return true;
}

static bool testMallocReallocFree() {
    int* test = malloc(sizeof(int) * 100);
    for (int i = 0; i < 100; i++) {
        test[i] = i*i;
    }
    test = realloc(test, sizeof(int) * 200);
    for (int i = 0; i < 100; i++) {
        ASSERT(test[i] == i*i);
    }
    free(test);
    return true;
}

static bool testGetSetUid() {
    int pid = fork();
    ASSERT(pid != -1);
    if (pid == 0) {
        ASSERT_CHILD(getuid() == 0);
        ASSERT_CHILD(setuid(1000) == 0);
        ASSERT_CHILD(getuid() == 1000);
        ASSERT_CHILD(setgid(1000) == -1);
        ASSERT_CHILD(setuid(1100) == -1);
        exit(0);
    } else {
        int status;
        int wait_pid = wait(&status);
        ASSERT(wait_pid == pid);
        ASSERT(WIFEXITED(status));
        ASSERT(WEXITSTATUS(status) == 0);
    }
    return true;
}

static bool testGetSetGid() {
    int pid = fork();
    ASSERT(pid != -1);
    if (pid == 0) {
        ASSERT_CHILD(getgid() == 0);
        ASSERT_CHILD(setgid(1000) == 0);
        ASSERT_CHILD(getgid() == 1000);
        ASSERT_CHILD(setuid(1000) == -0);
        ASSERT_CHILD(setgid(1100) == -1);
        exit(0);
    } else {
        int status;
        int wait_pid = wait(&status);
        ASSERT(wait_pid == pid);
        ASSERT(WIFEXITED(status));
        ASSERT(WEXITSTATUS(status) == 0);
    }
    return true;
}

static bool testPipe() {
    int fds[2][2];
    ASSERT(pipe(fds[0]) == 0);
    ASSERT(pipe(fds[1]) == 0);
    int pid = fork();
    ASSERT(pid != -1);
    if (pid == 0) {
        char buffer[512] = "Hello world!";
        ASSERT_CHILD(write(fds[0][1], buffer, 10) == 10);
        ASSERT_CHILD(read(fds[1][0], buffer, 512) == 10);
        ASSERT_CHILD(strncmp(buffer, "HELLO WORLD!", 10) == 0);
        exit(0);
    } else {
        char buffer[512] = "BUFFER INIT";
        ASSERT(read(fds[0][0], buffer, 512) == 10);
        ASSERT(strncmp(buffer, "Hello world!", 10) == 0);
        memcpy(buffer, "HELLO WORLD!", 10);
        ASSERT(write(fds[1][1], buffer, 10) == 10);
        int status;
        int wait_pid = wait(&status);
        ASSERT(wait_pid == pid);
        ASSERT(WIFEXITED(status));
        ASSERT(WEXITSTATUS(status) == 0);
    }
    close(fds[0][0]);
    close(fds[0][1]);
    close(fds[1][0]);
    close(fds[1][1]);
    return true;
}

static bool testPipeNonblock() {
    int fds[2];
    ASSERT(pipe(fds) == 0);
    int pid = fork();
    ASSERT(pid != -1);
    if (pid == 0) {
        fcntl(fds[0], F_SETFL, O_NONBLOCK);
        char buffer[512] = "BUFFER INIT";
        ASSERT_CHILD(read(fds[0], buffer, 512) == -1);
        ASSERT_CHILD(strcmp(buffer, "BUFFER INIT") == 0);
        read(fds[0], buffer, 512);
        ASSERT_CHILD(errno == EAGAIN);
        exit(0);
    } else {
        int status;
        int wait_pid = wait(&status);
        ASSERT(wait_pid == pid);
        ASSERT(WIFEXITED(status));
        ASSERT(WEXITSTATUS(status) == 0);
    }
    close(fds[0]);
    close(fds[1]);
    return true;
}

static bool testTtyNonblock() {
    int pid = fork();
    ASSERT(pid != -1);
    if (pid == 0) {
        fcntl(0, F_SETFL, O_NONBLOCK);
        char buffer[512] = "BUFFER INIT";
        ASSERT_CHILD(read(0, buffer, 512) == -1);
        ASSERT_CHILD(strcmp(buffer, "BUFFER INIT") == 0);
        read(0, buffer, 512);
        ASSERT_CHILD(errno == EAGAIN);
        exit(0);
    } else {
        int status;
        int wait_pid = wait(&status);
        ASSERT(wait_pid == pid);
        ASSERT(WIFEXITED(status));
        ASSERT(WEXITSTATUS(status) == 0);
    }
    return true;
}

static bool testPause() {
    int pid = fork();
    ASSERT(pid != -1);
    if (pid == 0) {
        pause();
        exit(42);
    } else {
        ASSERT(kill(pid, SIGUSR1) == 0);
        int status;
        int wait_pid = wait(&status);
        ASSERT(wait_pid == pid);
        ASSERT(WIFEXITED(status));
        ASSERT(!WIFSIGNALED(status));
        ASSERT(WEXITSTATUS(status) == 42);
    }
    return true;
}

static bool testPipeDup() {
    int fds[2];
    ASSERT(pipe(fds) == 0);
    int pid = fork();
    ASSERT(pid != -1);
    if (pid == 0) {
        dup2(fds[1], 1);
        execl("/bin/hello", "/bin/hello", NULL);
        exit(1);
    } else {
        char buffer[512];
        memset(buffer, 0, 512);
        while (read(fds[0], buffer, 512) > 0) {
            size_t i = 0;
            while (buffer[i] != 0 && buffer[i] != '\n') {
                i++;
            }
            if (buffer[i] == '\n') {
                buffer[i] = 0;
                break;
            }
        }
        ASSERT(strcmp(buffer, "Hello world!") == 0);
        int status;
        int wait_pid = wait(&status);
        ASSERT(wait_pid == pid);
        ASSERT(WIFEXITED(status));
        ASSERT(WEXITSTATUS(status) == 0);
    }
    close(fds[0]);
    close(fds[1]);
    return true;
}

static bool testAccess() {
    ASSERT(access("/bin/hello", F_OK) == 0);
    ASSERT(access("/bin/does_not_exist", F_OK) != 0);
    ASSERT(access("/bin/hello", R_OK | W_OK | X_OK) == 0);
    ASSERT(access("/include/stdlib.h", X_OK) != 0);
    return true;
}

static int testFunction(int a) {
    return 2 * a;
}

static bool testProtect() {
    uint32_t* buffer = malloc(1 << 12);
    ASSERT(mprotect(buffer, 1 << 12, PROT_EXEC | PROT_READ | PROT_WRITE) == 0);
    buffer[0] = 0xff010113;
    buffer[1] = 0x00113423;
    buffer[2] = 0x00050613;
    buffer[3] = 0x00058513;
    buffer[4] = 0x000600e7;
    buffer[5] = 0x0015159b;
    buffer[6] = 0x00a5853b;
    buffer[7] = 0x00813083;
    buffer[8] = 0x01010113;
    buffer[9] = 0x00008067;
    asm volatile("fence.i");
    ASSERT(((int(*)(int(*)(int), int))buffer)(testFunction, 7) == 42)
    free(buffer);
    return true;
}

static bool testForkSegvWait() {
    int pid = fork();
    ASSERT(pid != -1);
    if (pid == 0) {
        uint32_t* buffer = malloc(1 << 12);
        ASSERT(mprotect(buffer, 1 << 12, PROT_READ) == 0);
        buffer[0] = 42;
        exit(0);
    } else {
        int status;
        int wait_pid = wait(&status);
        ASSERT(wait_pid == pid);
        ASSERT(!WIFEXITED(status));
        ASSERT(WIFSIGNALED(status));
        ASSERT(WTERMSIG(status) == SIGSEGV);
    }
    return true;
}

static bool testMkdir() {
    ASSERT(mkdir("tmp", 0777) == 0);
    ASSERT(access("tmp", F_OK) == 0);
    return true;
}

static bool testOpenWriteClose() {
    int fd = open("tmp/test.txt", O_CREAT | O_WRONLY | O_EXCL);
    ASSERT(fd != -1);
    char buffer[512] = "Hello world!";
    ASSERT(write(fd, buffer, 12) == 12);
    close(fd);
    return true;
}

static bool testOpenReadClose() {
    int fd = open("/tmp/test.txt", O_RDONLY);
    ASSERT(fd != -1);
    char buffer[512] = "????????????";
    ASSERT(read(fd, buffer, 512) == 12);
    buffer[12] = 0;
    ASSERT(strcmp(buffer, "Hello world!") == 0)
    close(fd);
    return true;
}

static bool testOpenExcl() {
    ASSERT(open("/tmp/test_new.txt", O_CREAT | O_EXCL) != -1);
    ASSERT(open("/tmp/test_new.txt", O_CREAT | O_EXCL) == -1);
    ASSERT(remove("/tmp/test_new.txt") == 0)
    return true;
}

static bool testLink() {
    ASSERT(link("/tmp/test.txt", "/tmp/test2.txt") == 0);
    int fd = open("/tmp/test2.txt", O_RDONLY);
    ASSERT(fd != -1);
    char buffer[512] = "????????????";
    ASSERT(read(fd, buffer, 512) == 12);
    buffer[12] = 0;
    ASSERT(strcmp(buffer, "Hello world!") == 0)
    close(fd);
    return true;
}

static bool testReadDir() {
    DIR* dir = opendir("tmp");
    ASSERT(dir != NULL);
    struct dirent* entr = readdir(dir);
    ASSERT(entr != NULL);
    ASSERT(strcmp(entr->d_name, ".") == 0);
    entr = readdir(dir);
    ASSERT(entr != NULL);
    ASSERT(strcmp(entr->d_name, "..") == 0);
    entr = readdir(dir);
    ASSERT(entr != NULL);
    ASSERT(strcmp(entr->d_name, "test.txt") == 0);
    entr = readdir(dir);
    ASSERT(entr != NULL);
    ASSERT(strcmp(entr->d_name, "test2.txt") == 0);
    entr = readdir(dir);
    ASSERT(entr == NULL);
    closedir(dir);
    return true;
}

static bool testRename() {
    ASSERT(rename("/tmp/test.txt", "/tmp/test3.txt") == 0);
    int fd = open("/tmp/test3.txt", O_RDONLY);
    ASSERT(fd != -1);
    char buffer[512] = "????????????";
    ASSERT(read(fd, buffer, 512) == 12);
    buffer[12] = 0;
    ASSERT(strcmp(buffer, "Hello world!") == 0)
    close(fd);
    return true;
}

static bool testReadDir2() {
    DIR* dir = opendir("tmp");
    ASSERT(dir != NULL);
    struct dirent* entr = readdir(dir);
    ASSERT(entr != NULL);
    ASSERT(strcmp(entr->d_name, ".") == 0);
    entr = readdir(dir);
    ASSERT(entr != NULL);
    ASSERT(strcmp(entr->d_name, "..") == 0);
    entr = readdir(dir);
    ASSERT(entr != NULL);
    ASSERT(strcmp(entr->d_name, "test3.txt") == 0);
    entr = readdir(dir);
    ASSERT(entr != NULL);
    ASSERT(strcmp(entr->d_name, "test2.txt") == 0);
    entr = readdir(dir);
    ASSERT(entr == NULL);
    closedir(dir);
    return true;
}

static bool testGetSetCwd() {
    char buffer[512];
    ASSERT(getcwd(buffer, 512) != (void*)-1)
    ASSERT(strcmp(buffer, "/") == 0);
    ASSERT(chdir("./tmp") == 0);
    ASSERT(getcwd(buffer, 512) != (void*)-1)
    ASSERT(strcmp(buffer, "/tmp") == 0);
    return true;
}

static bool testOpenRelative() {
    char buffer[512] = "????????????";
    int fd = open("./test3.txt", O_RDONLY);
    ASSERT(fd != -1);
    ASSERT(read(fd, buffer, 512) == 12);
    buffer[12] = 0;
    ASSERT(strcmp(buffer, "Hello world!") == 0)
    close(fd);
    return true;
}

static bool testOpenAbsolute() {
    char buffer[512] = "????????????";
    int fd = open("/tmp/test3.txt", O_RDONLY);
    ASSERT(fd != -1);
    ASSERT(read(fd, buffer, 512) == 12);
    buffer[12] = 0;
    ASSERT(strcmp(buffer, "Hello world!") == 0)
    close(fd);
    return true;
}

static bool testGetSetCwd2() {
    char buffer[512];
    ASSERT(chdir("/") == 0);
    ASSERT(getcwd(buffer, 512) != (void*)-1)
    ASSERT(strcmp(buffer, "/") == 0);
    ASSERT(open("./test3.txt", O_RDONLY) == -1);
    return true;
}

static bool testUnlinkDirFail() {
    ASSERT(unlink("/tmp") == -1);
    ASSERT(access("/tmp", F_OK) == 0);
    return true;
}

static bool testOpenSeekWriteClose() {
    int fd = open("/tmp/test3.txt", O_WRONLY);
    ASSERT(fd != -1);
    ASSERT(lseek(fd, 6, SEEK_SET) == 6);
    ASSERT(write(fd, "WORLD", 5) == 5);
    close(fd);
    char buffer[512] = "????????????";
    fd = open("/tmp/test2.txt", O_RDONLY);
    ASSERT(fd != -1);
    ASSERT(read(fd, buffer, 512) == 12);
    buffer[12] = 0;
    ASSERT(strcmp(buffer, "Hello WORLD!") == 0)
    close(fd);
    return true;
}

static bool testOpenAppendWriteClose() {
    int fd = open("/tmp/test2.txt", O_WRONLY | O_APPEND);
    ASSERT(fd != -1);
    ASSERT(write(fd, " Roland.", 8) == 8);
    close(fd);
    char buffer[512] = "????????????";
    fd = open("/tmp/test3.txt", O_RDONLY);
    ASSERT(fd != -1);
    ASSERT(read(fd, buffer, 512) == 20);
    buffer[20] = 0;
    ASSERT(strcmp(buffer, "Hello WORLD! Roland.") == 0)
    close(fd);
    return true;
}

static bool testOpenWriteReadClose() {
    char buffer[512] = "????????????";
    int fd = open("/tmp/test2.txt", O_RDWR);
    ASSERT(fd != -1);
    ASSERT(write(fd, "HELLO", 5) == 5);
    ASSERT(read(fd, buffer, 512) == 15);
    buffer[15] = 0;
    ASSERT(strcmp(buffer, " WORLD! Roland.") == 0);
    ASSERT(lseek(fd, 0, SEEK_SET) == 0);
    ASSERT(read(fd, buffer, 512) == 20);
    buffer[20] = 0;
    ASSERT(strcmp(buffer, "HELLO WORLD! Roland.") == 0);
    close(fd);
    return true;
}

static bool testTruncOpenReadClose() {
    char buffer[512] = "????????????";
    ASSERT(truncate("/tmp/test2.txt", 12) == 0);
    int fd = open("/tmp/test2.txt", O_RDONLY);
    ASSERT(fd != -1);
    ASSERT(read(fd, buffer, 512) == 12);
    buffer[12] = 0;
    ASSERT(strcmp(buffer, "HELLO WORLD!") == 0);
    close(fd);
    return true;
}

static bool testOpenTruncReadClose() {
    char buffer[512] = "????????????";
    int fd = open("/tmp/test2.txt", O_RDWR);
    ASSERT(fd != -1);
    ASSERT(ftruncate(fd, 5) == 0);
    ASSERT(read(fd, buffer, 512) == 5);
    buffer[5] = 0;
    ASSERT(strcmp(buffer, "HELLO") == 0);
    close(fd);
    return true;
}

static bool testOpenTruncWriteClose() {
    int fd = open("/tmp/test2.txt", O_WRONLY | O_TRUNC);
    ASSERT(fd != -1);
    ASSERT(write(fd, "Hello world!", 12) == 12);
    close(fd);
    char buffer[512] = "????????????";
    fd = open("/tmp/test3.txt", O_RDONLY);
    ASSERT(fd != -1);
    ASSERT(read(fd, buffer, 512) == 12);
    buffer[12] = 0;
    ASSERT(strcmp(buffer, "Hello world!") == 0)
    close(fd);
    return true;
}

static bool testStatReg() {
    struct stat stats;
    ASSERT(stat("/tmp/test2.txt", &stats) == 0);
    ASSERT((stats.st_mode & S_IFMT) == S_IFREG);
    ASSERT(stats.st_nlink == 2);
    ASSERT(stats.st_size == 12);
    return true;
}

static bool testStatDir() {
    struct stat stats;
    ASSERT(stat("/tmp", &stats) == 0);
    ASSERT((stats.st_mode & S_IFMT) == S_IFDIR);
    return true;
}

static bool testStatChr() {
    struct stat stats;
    ASSERT(stat("/dev/tty0", &stats) == 0);
    ASSERT((stats.st_mode & S_IFMT) == S_IFCHR);
    return true;
}

static bool testStatBlk() {
    struct stat stats;
    ASSERT(stat("/dev/blk0", &stats) == 0);
    ASSERT((stats.st_mode & S_IFMT) == S_IFBLK);
    return true;
}

static bool testChmodStat() {
    ASSERT(chmod("/tmp/test2.txt", 0777) == 0);
    struct stat stats;
    ASSERT(stat("/tmp/test2.txt", &stats) == 0);
    ASSERT((stats.st_mode & S_IFMT) == S_IFREG);
    ASSERT((stats.st_mode & S_IRWXU) == S_IRWXU);
    ASSERT((stats.st_mode & S_IRWXG) == S_IRWXG);
    ASSERT((stats.st_mode & S_IRWXO) == S_IRWXO);
    return true;
}

static bool testChownStat() {
    ASSERT(chown("/tmp/test2.txt", 1000, 1100) == 0);
    struct stat stats;
    ASSERT(stat("/tmp/test2.txt", &stats) == 0);
    ASSERT(stats.st_uid == 1000);
    ASSERT(stats.st_gid == 1100);
    return true;
}

static bool testDup() {
    char buffer[512] = "????????????";
    int fd = open("/tmp/test3.txt", O_RDONLY);
    ASSERT(fd != -1);
    int fd2 = dup(fd);
    ASSERT(fd2 != -1);
    ASSERT(read(fd, buffer, 512) == 12);
    buffer[12] = 0;
    ASSERT(strcmp(buffer, "Hello world!") == 0)
    memset(buffer, '?', 12);
    ASSERT(read(fd2, buffer, 512) == 0);
    ASSERT(lseek(fd, 0, SEEK_SET) == 0);
    close(fd);
    ASSERT(read(fd2, buffer, 512) == 12);
    buffer[12] = 0;
    ASSERT(strcmp(buffer, "Hello world!") == 0)
    close(fd2);
    return true;
}

static bool testFcntlDup() {
    char buffer[512] = "????????????";
    int fd = open("/tmp/test3.txt", O_RDONLY);
    ASSERT(fd != -1);
    int fd2 = fcntl(fd, F_DUPFD, 5000);
    ASSERT(fd2 == 5000);
    ASSERT(read(fd, buffer, 512) == 12);
    buffer[12] = 0;
    ASSERT(strcmp(buffer, "Hello world!") == 0)
    memset(buffer, '?', 12);
    ASSERT(read(fd2, buffer, 512) == 0);
    ASSERT(lseek(fd, 0, SEEK_SET) == 0);
    close(fd);
    ASSERT(read(fd2, buffer, 512) == 12);
    buffer[12] = 0;
    ASSERT(strcmp(buffer, "Hello world!") == 0)
    close(fd2);
    return true;
}

static bool testFcntlGetFlags() {
    ASSERT((fcntl(0, F_GETFD) & FD_CLOEXEC) == 0);
    ASSERT((fcntl(0, F_GETFL) & O_ACCMODE) == FREAD);
    ASSERT((fcntl(1, F_GETFD) & FD_CLOEXEC) == 0);
    ASSERT((fcntl(1, F_GETFL) & O_ACCMODE) == FWRITE);
    ASSERT((fcntl(2, F_GETFD) & FD_CLOEXEC) == 0);
    ASSERT((fcntl(2, F_GETFL) & O_ACCMODE) == FWRITE);
    int fd = open("/tmp/test3.txt", O_RDONLY);
    ASSERT(fd != -1);
    ASSERT((fcntl(fd, F_GETFD) & FD_CLOEXEC) == 0);
    ASSERT((fcntl(fd, F_GETFL) & O_ACCMODE) == FREAD);
    close(fd);
    fd = open("/tmp/test3.txt", O_WRONLY | O_CLOEXEC);
    ASSERT(fd != -1);
    ASSERT((fcntl(fd, F_GETFD) & FD_CLOEXEC) != 0);
    ASSERT((fcntl(fd, F_GETFL) & O_ACCMODE) == FWRITE);
    close(fd);
    fd = open("/tmp/test3.txt", O_RDWR | O_NONBLOCK);
    ASSERT(fd != -1);
    ASSERT((fcntl(fd, F_GETFD) & FD_CLOEXEC) == 0);
    ASSERT((fcntl(fd, F_GETFL) & (O_ACCMODE | O_NONBLOCK)) == (FREAD | FWRITE | FNONBLOCK));
    close(fd);
    return true;
}

static bool testIsatty() {
    ASSERT(isatty(0) == 1);
    ASSERT(isatty(1) == 1);
    ASSERT(isatty(2) == 1);
    int fd = open("/tmp/test3.txt", O_RDONLY);
    ASSERT(fd != -1);
    int res = isatty(fd);
    ASSERT(errno == ENOTTY);
    ASSERT(res == 0);
    close(fd);
    res = isatty(54321);
    ASSERT(errno == EBADF);
    ASSERT(res == 0);
    return true;
}

static bool testUnlinkFile() {
    char buffer[512] = "????????????";
    ASSERT(unlink("/tmp/test2.txt") == 0);
    ASSERT(access("/tmp/test2.txt", F_OK) == -1);
    int fd = open("/tmp/test3.txt", O_RDONLY);
    ASSERT(fd != -1);
    ASSERT(read(fd, buffer, 512) == 12);
    buffer[12] = 0;
    ASSERT(strcmp(buffer, "Hello world!") == 0)
    close(fd);
    ASSERT(unlink("/tmp/test3.txt") == 0);
    ASSERT(access("/tmp/test2.txt", F_OK) == -1);
    ASSERT(open("/tmp/test3.txt", O_RDONLY) == -1);
    return true;
}

static bool testMount() {
    ASSERT(mount("/dev/blk0", "/tmp", "minix", 0, NULL) == 0);
    ASSERT(access("/tmp/bin/hello", F_OK) == 0);
    return true;
}

static bool testUmount() {
    ASSERT(umount("/tmp") == 0);
    ASSERT(access("/tmp/bin/hello", F_OK) != 0);
    return true;
}

static bool testUnlinkDir() {
    ASSERT(unlink("/tmp") == 0);
    ASSERT(access("/tmp", F_OK) == -1);
    return true;
}

static bool testUmask() {
    umask(0);
    ASSERT(umask(0002) == 0000);
    ASSERT(umask(0222) == 0002);
    ASSERT(umask(0022) == 0222);
    return true;
}

static bool testUmaskOpen() {
    umask(0022);
    int fd = open("test_file.txt", O_CREAT, 0777);
    ASSERT(fd != -1);
    struct stat stat_buf;
    ASSERT(fstat(fd, &stat_buf) == 0);
    ASSERT(stat_buf.st_mode == (S_IFREG | 0755));
    ASSERT(close(fd) == 0);
    ASSERT(remove("test_file.txt") == 0);
    return true;
}

static bool testUmaskMkdir() {
    umask(0023);
    mkdir("test_dir", 0777);
    struct stat stat_buf;
    ASSERT(stat("test_dir", &stat_buf) == 0);
    ASSERT(stat_buf.st_mode == (S_IFDIR | 0754));
    ASSERT(remove("test_dir") == 0);
    return true;
}

static bool testUmaskFork() {
    umask(0123);
    ASSERT(umask(0123) == 0123);
    int pid = fork();
    ASSERT(pid != -1);
    if (pid == 0) {
        ASSERT_CHILD(umask(0321) == 0123);
        exit(0);
    } else {
        int status;
        int wait_pid = wait(&status);
        ASSERT(wait_pid == pid);
        ASSERT(WIFEXITED(status));
        ASSERT(WEXITSTATUS(status) == 0);
        ASSERT(umask(0231) == 0123);
    }
    return true;
}

static bool testGettimeofday() {
    struct timeval start;
    ASSERT(gettimeofday(&start, NULL) == 0);
    usleep(10000);
    struct timeval end;
    ASSERT(gettimeofday(&end, NULL) == 0);
    ASSERT((start.tv_sec * 1000000 + start.tv_usec) + 5000 < (end.tv_sec * 1000000 + end.tv_usec));
    ASSERT((start.tv_sec * 1000000 + start.tv_usec) + 20000 > (end.tv_sec * 1000000 + end.tv_usec));
    return true;
}

static bool testTime() {
    struct timeval tv;
    ASSERT(gettimeofday(&tv, NULL) == 0);
    ASSERT(tv.tv_sec + 10 >= time(NULL) && tv.tv_sec <= time(NULL) + 10);
    return true;
}

static bool testSettimeofday() {
    struct timeval start = { .tv_sec = 123456, .tv_usec = 654321 };
    ASSERT(settimeofday(&start, NULL) == 0);
    usleep(10000);
    struct timeval end;
    ASSERT(gettimeofday(&end, NULL) == 0);
    ASSERT((start.tv_sec * 1000000 + start.tv_usec) + 5000 < (end.tv_sec * 1000000 + end.tv_usec));
    ASSERT((start.tv_sec * 1000000 + start.tv_usec) + 20000 > (end.tv_sec * 1000000 + end.tv_usec));
    return true;
}

typedef bool (*TestFunction)();

typedef struct {
    const char* name;
    TestFunction func;
} TestCase;

#define TEST(FUNC) { .name = #FUNC, .func = FUNC }

static bool runBasicSyscallTests() {
    static const TestCase tests[] = {
        TEST(testSyscallYield),
        TEST(testForkExitWait),
        TEST(testForkWaitNohang),
        TEST(testSigStopCont),
        TEST(testForkExecWait),
        TEST(testClock),
        TEST(testUsleep),
        TEST(testNanosleep),
        TEST(testForkKillWait),
        TEST(testForkSleepKillWait),
        TEST(testSignal),
        TEST(testGetpid),
        TEST(testGetppid),
        TEST(testMallocReallocFree),
        TEST(testGetSetUid),
        TEST(testGetSetGid),
        TEST(testPipe),
        TEST(testPipeNonblock),
        TEST(testTtyNonblock),
        TEST(testPause),
        TEST(testPipeDup),
        TEST(testAccess),
        TEST(testProtect),
        TEST(testForkSegvWait),
        TEST(testMkdir),
        TEST(testOpenWriteClose),
        TEST(testOpenReadClose),
        TEST(testOpenExcl),
        TEST(testLink),
        TEST(testReadDir),
        TEST(testRename),
        TEST(testReadDir2),
        TEST(testGetSetCwd),
        TEST(testOpenRelative),
        TEST(testOpenAbsolute),
        TEST(testGetSetCwd2),
        TEST(testUnlinkDirFail),
        TEST(testOpenSeekWriteClose),
        TEST(testOpenAppendWriteClose),
        TEST(testOpenWriteReadClose),
        TEST(testTruncOpenReadClose),
        TEST(testOpenTruncReadClose),
        TEST(testOpenTruncWriteClose),
        TEST(testStatReg),
        TEST(testStatDir),
        TEST(testStatChr),
        TEST(testStatBlk),
        TEST(testChmodStat),
        TEST(testChownStat),
        TEST(testDup),
        TEST(testFcntlDup),
        TEST(testFcntlGetFlags),
        TEST(testIsatty),
        TEST(testUnlinkFile),
        TEST(testMount),
        TEST(testUmount),
        TEST(testUnlinkDir),
        TEST(testUmask),
        TEST(testUmaskOpen),
        TEST(testUmaskMkdir),
        TEST(testUmaskFork),
        TEST(testGettimeofday),
        TEST(testTime),
        TEST(testSettimeofday),
    };
    bool result = true;
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        if (tests[i].func()) {
            USPACE_SUBSUCCESS("Passed %s", tests[i].name);
        } else {
            result = false;
        }
    }
    return result;
}

int main() {
    if (runBasicSyscallTests()) {
        return EXIT_SUCCESS;
    } else {
        return EXIT_FAILURE;
    }
}

