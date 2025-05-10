// fiber.h
#pragma once
#include <stdint.h>
#include <ucontext.h>

typedef struct Fiber {
    ucontext_t      ctx;            // полный контекст
    struct Fiber*   next;
    void          (*f)(void*);
    void*           args;
    uint8_t*        stack;
    int             finished;
} Fiber;

void FiberInit(void);
void FiberSpawn(void (*f)(void*), void* args);
void FiberYield(void);
int  FiberTryJoin(void);
