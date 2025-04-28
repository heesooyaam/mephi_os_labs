#pragma once
#include <stdint.h>
#include "atomic.h"

struct SpinLock {
    int64_t atomic_locked;
};

inline void SpinLock_Init(struct SpinLock* lock) {
    lock->atomic_locked = 0;
}

inline void SpinLock_Lock(struct SpinLock* lock) {
    while (AtomicXchg(&lock->atomic_locked, 1)) {}
}

inline void SpinLock_Unlock(struct SpinLock* lock) {
    AtomicXchg(&lock->atomic_locked, 0);
}