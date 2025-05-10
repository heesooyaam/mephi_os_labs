// fiber.h
#pragma once
#include <stdint.h>
#include <ucontext.h>

void FiberInit(void);
void FiberSpawn(void (*f)(void*), void* args);
void FiberYield(void);
int  FiberTryJoin(void);
