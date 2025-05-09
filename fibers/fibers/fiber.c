#include "fiber.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define FIBER_STACK_SIZE (64 * 1024)

struct Fiber {
    uint64_t rsp;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;

    struct Fiber* next;
    void (*f)(void*);
    void*    args;
    uint8_t* stack;
    int      finished;
};

extern void SwitchFiberContext(struct Fiber* from, struct Fiber* to);
static struct Fiber* CurrentFiber = NULL;

static void InitMainFiber() {
    if (CurrentFiber) {
        return;
    }

    static struct Fiber Main = {0};
    Main.next = &Main;
    CurrentFiber = &Main;
}

static void FiberTrampoline() {
    CurrentFiber->f(CurrentFiber->args);
    CurrentFiber->finished = 1;
    FiberYield();

    // should never reach
    assert(0);
}

// static uint64_t PrepareStack(uint8_t* stack_top) {
//     uintptr_t sp = (uintptr_t)stack_top;
//     sp &= ~0xF;
//
//     sp -= 8;              /* фиктивная ячейка */
//     *(uint64_t*)sp = 0;
//
//     sp -= 8;              /* return-адрес для ret */
//     *(uint64_t*)sp = (uint64_t)FiberTrampoline;
//
//     return (uint64_t)sp;
// }

static uint64_t PrepareStack(uint8_t* stack_top) {
    uintptr_t rsp = (uintptr_t) stack_top;
    // rsp &= ~((1 << 4) - 1);
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
    fiber->rsp = PrepareStack(fiber->stack + FIBER_STACK_SIZE);
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
    SwitchFiberContext(from, to);
}

int FiberTryJoin() {
    InitMainFiber();
    FreeFinishedFibers();
    return CurrentFiber->next == CurrentFiber;
}
