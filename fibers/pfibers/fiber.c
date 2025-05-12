#include "fiber.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ucontext.h>

#define FIBER_STACK_SIZE (64 << 10)

struct Context {
    size_t r8, r9, r10, r11, r12, r13, r14, r15;
    size_t rdi, rsi, rbp, rbx, rdx, rax, rcx, rsp;
    size_t rip, flags;
};

struct Fiber {
    struct Context context;

    struct Fiber* next;
    void (*f)(void*);
    void*    args;
    uint8_t* stack;
    int      finished;
};

extern void SwitchFiberContext(struct Context* from, struct Context* to);
static struct Fiber* CurrentFiber = NULL;

static void InitMainFiber() {
    if (CurrentFiber) {
        return;
    }

    static struct Fiber Main = {0};
    Main.next = &Main;
    Main.f = NULL;
    CurrentFiber = &Main;
}

static void FiberTrampoline() {
    CurrentFiber->f(CurrentFiber->args);
    CurrentFiber->finished = 1;

    FiberYield();

    // should never reach
    assert(0);
}

static uint64_t PrepareStack(uint8_t* stack_top) {
    uintptr_t rsp = (uintptr_t) stack_top;
    rsp &= ~((1 << 4) - 1);
    *(uint64_t*)(rsp - 16) = (uintptr_t) FiberTrampoline;
    return rsp - 16;
}

void FiberSpawn(void (*f)(void*), void* args) {
    if (!f) {
        return;
    }

    InitMainFiber();

    struct Fiber* fiber = malloc(sizeof(struct Fiber));
    fiber->f = f;
    fiber->args = args;
    fiber->finished = 0;
    fiber->stack = malloc(FIBER_STACK_SIZE);
    fiber->context.rsp = PrepareStack(fiber->stack + FIBER_STACK_SIZE);
    fiber->next = CurrentFiber;

    struct Fiber* tail = CurrentFiber;
    while (tail->next != CurrentFiber) {
        tail = tail->next;
    }
    tail->next = fiber;
}

static void FreeFinishedFibers() {
    struct Fiber* prev = CurrentFiber;
    struct Fiber* cur = prev->next;

    while (cur != CurrentFiber) {
        if (cur->finished) {
            prev->next = cur->next;
            free(cur->stack);
            free(cur);

            cur = prev->next;
        } else {
            prev = cur;
            cur = cur->next;
        }
    }
}

void FiberYield() {
    InitMainFiber();
    FreeFinishedFibers();

    struct Fiber* from = CurrentFiber;
    struct Fiber* to   = CurrentFiber->next;

    if (to == from) {
        return;
    }

    CurrentFiber = to;
    SwitchFiberContext(&(from->context), &(to->context));
}

int FiberTryJoin() {
    InitMainFiber();
    FreeFinishedFibers();
    return CurrentFiber->next == CurrentFiber;
}

void fiber_sched(int signum, siginfo_t *si, void *ucontext) {
    // если один файбер, то ничего делать не надо, продолжаем в том же духе
    if (CurrentFiber->next == CurrentFiber) {
        return;
    }

    FiberYield();
    ucontext_t *uc = (ucontext_t *)ucontext;
    // кладем rip на стек, поддерживая инвариант
    memcpy(&CurrentFiber->context, uc->uc_mcontext.gregs, sizeof(struct Context));
    uint64_t *rsp = (uint64_t *)CurrentFiber->context.rsp - 1;
    *rsp = CurrentFiber->context.rip;
    CurrentFiber->context.rsp = (size_t) rsp;

    // меняем контекст
    CurrentFiber = CurrentFiber->next;
    memcpy(uc->uc_mcontext.gregs, &(CurrentFiber->context), sizeof(struct Context));
    uint64_t *new_rsp = (uint64_t *)CurrentFiber->context.rsp;
    ((struct Context*)uc->uc_mcontext.gregs)->rip = *new_rsp;
    ((struct Context*)uc->uc_mcontext.gregs)->rsp = (greg_t)(new_rsp + 1); // pop rip
}

void FiberInit() {
    // Need to disable stdlib buffer to use printf from fibers
    setbuf(stdout, NULL);

    // Initialize timer
    struct sigaction sa = {0};
    sa.sa_sigaction = fiber_sched;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;

    sigaction(SIGALRM, &sa, NULL);
    ualarm(1000, 1000);
}
