#include "syscall.h"

#include <asm/unistd.h>

int open(const char* pathname, int flags) {
    int volatile ret;
    asm volatile (
        "syscall"
        : "=a" (ret)
        : "0" (__NR_open), "D" (pathname), "S" ((ssize_t) flags)
        : "%rcx", "%r11", "memory"
    );

    return ret;
}

int close(int fd) {
    int volatile ret;
    asm volatile (
        "syscall"
        : "=a" (ret)
        : "0" (__NR_close), "D" ((ssize_t) fd)
        : "%rcx", "%r11", "memory"
    );

    return ret;
}

ssize_t read(int fd, void* buf, size_t count) {
    int volatile ret;
    asm volatile (
        "syscall"
        : "=a" (ret)
        : "0" (__NR_read), "D"((ssize_t) fd), "S" (buf), "d" (count)
        : "%rcx", "%r11", "memory"
    );

    return ret;
}

ssize_t write(int fd, const void* buf, ssize_t count) {
    int volatile ret;
    asm volatile (
        "syscall"
        : "=a" (ret)
        : "0" (__NR_write), "D"((ssize_t) fd), "S" (buf), "d" (count)
        : "%rcx", "%r11", "memory"
    );

    return ret;
}

int pipe(int pipefd[2]) {
    int volatile ret;
    asm volatile (
        "syscall"
        : "=a" (ret)
        : "0" (__NR_pipe), "D"(pipefd)
        : "%rcx", "%r11", "memory"
    );

    return ret;
}

int dup(int oldfd) {
    pid_t volatile new_fd;
    asm volatile (
        "syscall"
        : "=a" (new_fd)
        : "0" (__NR_dup), "D" ((ssize_t) oldfd)
        : "%rcx", "%r11", "memory"
    );

    return new_fd;
}

pid_t fork() {
    pid_t volatile ch_pid;
    asm volatile (
        "syscall"
        : "=a" (ch_pid)
        : "0" (__NR_fork)
        : "%rcx", "%r11", "memory"
    );

    return ch_pid;
}


pid_t waitpid(pid_t pid, int* wstatus, int options) {
    pid_t volatile stopped_pid;
    register long r10 asm("r10") = 0;
    asm volatile (
        "syscall"
        : "=a" (stopped_pid)
        : "0" (__NR_wait4), "D"((ssize_t) pid), "S" (wstatus), "d" ((ssize_t) options), "r" (r10)
        : "%rcx", "%r11", "memory"
    );

    return stopped_pid;
}

int execve(const char* filename, char* const argv[], char* const envp[]) {
    ssize_t volatile ret;
    asm volatile (
        "syscall"
        : "=a" (ret)
        : "a" (__NR_execve), "D" (filename), "S" (argv), "d" (envp)
        : "%rcx", "%r11", "memory"
    );

    return ret;
}

void exit(int status) {
    asm volatile (
        "syscall"
        :
        : "a" (__NR_exit), "D" ((ssize_t) status)
        : "%rcx", "%r11", "memory"
    );
}
