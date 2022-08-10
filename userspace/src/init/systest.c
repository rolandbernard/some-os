
#include <stdint.h>
#include <stdlib.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "systest.h"
#include "log.h"

#define ASSERT(COND)                                \
    if (!(COND)) {                                  \
        USPACE_ERROR("Failed assertion: " #COND);   \
        return false;                               \
    }

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
    ASSERT(getpid() == 1);
    return true;
}

static bool testGetppid() {
    ASSERT(getppid() == 0);
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
        ASSERT_CHILD(setgid(1000) == 0);
        ASSERT_CHILD(setuid(1100) == -1);
        exit(0);
    } else {
        int status;
        int wait_pid = wait(&status);
        return wait_pid == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0;
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
        ASSERT_CHILD(setuid(1000) == 0);
        ASSERT_CHILD(setgid(1100) == -1);
        exit(0);
    } else {
        int status;
        int wait_pid = wait(&status);
        return wait_pid == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0;
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
        return wait_pid == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
    close(fds[0][0]);
    close(fds[0][1]);
    close(fds[1][0]);
    close(fds[1][1]);
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
        return wait_pid == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
    close(fds[0]);
    close(fds[1]);
    return true;
}

typedef bool (*TestFunction)();

typedef struct {
    const char* name;
    TestFunction func;
} TestCase;

#define TEST(FUNC) { .name = #FUNC, .func = FUNC }

bool runBasicSyscallTests() {
    static const TestCase tests[] = {
        TEST(testSyscallYield),
        TEST(testForkExitWait),
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
        TEST(testPause),
        TEST(testPipeDup),
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

