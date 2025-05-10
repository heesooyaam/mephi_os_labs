// fiber.c
#define _GNU_SOURCE
#include "fiber.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <ucontext.h>

#include <stdint.h>

#define FIBER_STACK_SIZE (64*1024)

typedef struct Fiber {
    ucontext_t      ctx;            // полный контекст
    struct Fiber*   next;
    void          (*f)(void*);
    void*           args;
    uint8_t*        stack;
    int             finished;
} Fiber;

static Fiber* Current = NULL;

// forward
static void fiber_sched(int, siginfo_t*, void*);
static void FiberTrampoline(void);

// Инициализируем Main-файбер и регистрируем SIGALRM
void FiberInit(void) {
    if (Current) return;
    setbuf(stdout, NULL);

    // создаём Main
    Current = malloc(sizeof *Current);
    memset(Current,0,sizeof *Current);
    Current->stack = NULL;
    Current->finished = 0;
    Current->next = Current;

    // захватываем текущий контекст как стартовый
    getcontext(&Current->ctx);

    // устанавливаем sigaction для вытеснения
    struct sigaction sa = {0};
    sa.sa_sigaction = fiber_sched;
    sa.sa_flags     = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGALRM, &sa, NULL) < 0) { perror("sigaction"); exit(1); }
    if (ualarm(1000, 1000) != 0)  { fprintf(stderr,"ualarm\n"); exit(1); }
}

void FiberSpawn(void (*f)(void*), void* args) {
    assert(f);
    FiberInit();

    Fiber* fbr = malloc(sizeof *fbr);
    memset(fbr,0,sizeof *fbr);
    fbr->f    = f;
    fbr->args = args;
    fbr->finished = 0;
    fbr->stack = malloc(FIBER_STACK_SIZE);

    // подготовка контекста
    getcontext(&fbr->ctx);
    fbr->ctx.uc_stack.ss_sp   = fbr->stack;
    fbr->ctx.uc_stack.ss_size = FIBER_STACK_SIZE;
    fbr->ctx.uc_link          = NULL;    // при завершении — никуда
    makecontext(&fbr->ctx, FiberTrampoline, 0);

    // вставка в кольцо
    Fiber* tail = Current;
    while (tail->next != Current) tail = tail->next;
    tail->next = fbr;
    fbr->next  = Current;
}

static void FreeFinished(void) {
    Fiber* prev = Current;
    Fiber* cur  = prev->next;
    while (cur != Current) {
        if (cur->finished) {
            prev->next = cur->next;
            free(cur->stack);
            free(cur);
            cur = prev->next;
        } else {
            prev = cur;
            cur  = cur->next;
        }
    }
}

void FiberYield(void) {
    FiberInit();
    FreeFinished();

    Fiber* next = Current->next;
    if (next == Current) return;
    Fiber* from = Current;
    Current = next;
    // поменяем контексты
    swapcontext(&from->ctx, &next->ctx);
}

int FiberTryJoin(void) {
    FiberInit();
    FreeFinished();
    return Current->next == Current;
}

// точка входа для любого нового файбера
static void FiberTrampoline(void) {
    Fiber* self = Current;
    self->f(self->args);
    self->finished = 1;
    // не возвращаемся никогда сюда
    FiberYield();
    abort();
}

// вытесняющий шедулер
static void fiber_sched(int sig, siginfo_t *si, void *uap) {
    ucontext_t *uc = (ucontext_t*)uap;

    // Сохраняем весь текущий контекст в Current->ctx
    Current->ctx = *uc;

    FreeFinished();
    Fiber* to = Current->next;
    if (to == Current) {
        // нет другого — остаёмся
        return;
    }
    Current = to;
    memcpy(uap, &Current->ctx, sizeof(ucontext_t));
    // и незамедлительно переключаемся на новый
    // setcontext не возвращает
    abort();
}
