#pragma once
#include <stdint.h>
#include "spinlock.h"

#pragma once

struct RwLock {
    struct SpinLock write_m;
    int64_t atomic_readers_cnt;
};

inline void RwLock_Init(struct RwLock* lock) {
    SpinLock_Init(&lock->write_m);
    lock->atomic_readers_cnt = 0;
}

inline void RwLock_ReadLock(struct RwLock* lock) {
    for (;;) {
        while (AtomicLoad(&lock->write_m.atomic_locked)) {}

        AtomicAdd(&lock->atomic_readers_cnt, 1);
        __sync_synchronize();
        if (!AtomicLoad(&lock->write_m.atomic_locked)) {
            break;
        }
        AtomicSub(&lock->atomic_readers_cnt, 1);
    }
}

inline void RwLock_ReadUnlock(struct RwLock* lock) {
    __sync_synchronize();
    AtomicSub(&lock->atomic_readers_cnt, 1);
}

inline void RwLock_WriteLock(struct RwLock* lock) {
    SpinLock_Lock(&lock->write_m);
    while (AtomicLoad(&lock->atomic_readers_cnt)) {}
}

inline void RwLock_WriteUnlock(struct RwLock* lock) {
    __sync_synchronize();
    SpinLock_Unlock(&lock->write_m);
}
