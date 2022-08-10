
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

bool testSyscallYield() {
    // Test that it does not crash.
    syscall0(2);
    return true;
}

bool testForkExitWait() {
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

bool testForkExecWait() {
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

bool testClock() {
    clock_t t = clock();
    ASSERT(clock() >= t);
    ASSERT(clock() > t);
    return true;
}

bool testUsleep() {
    clock_t t1 = times(NULL);
    ASSERT(usleep(10000) == 0);
    clock_t t2 = times(NULL);
    ASSERT(t2 >= t1 + CLOCKS_PER_SEC / 100);
    ASSERT(t2 <= t1 + CLOCKS_PER_SEC / 50);
    return true;
}

bool testNanosleep() {
    clock_t t1 = times(NULL);
    struct timespec t = {
        .tv_sec = 0, .tv_nsec = 10000000
    };
    ASSERT(nanosleep(&t, &t) == 0)
    clock_t t2 = times(NULL);
    ASSERT(t2 >= t1 + CLOCKS_PER_SEC / 100);
    ASSERT(t2 <= t1 + CLOCKS_PER_SEC / 50);
    return true;
}

bool testForkKillWait() {
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
    };
    bool result = true;
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        if (tests[i].func()) {
            USPACE_SUBSUCCESS("Passed %s", tests[i].name);
        }
    }
    return result;
}

