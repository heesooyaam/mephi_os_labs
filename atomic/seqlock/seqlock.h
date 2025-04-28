#pragma once

#include <stdint.h>
#include "spinlock.h"
#include "atomic.h"

struct SeqLock {
    struct SpinLock lock;
    int64_t atomic_seq;
};

inline void SeqLock_Init(struct SeqLock* sl) {
    SpinLock_Init(&sl->lock);
    sl->atomic_seq = 0;
}

inline int64_t SeqLock_ReadLock(struct SeqLock* sl) {
    return AtomicLoad(&sl->atomic_seq);
}

inline int SeqLock_ReadUnlock(struct SeqLock* sl, int64_t start) {
    __sync_synchronize();
    int64_t end = AtomicLoad(&sl->atomic_seq);
    return (end == start) && ((start & 1) == 0);
}

inline void SeqLock_WriteLock(struct SeqLock* sl) {
    SpinLock_Lock(&sl->lock);
    __sync_synchronize();
    AtomicAdd(&sl->atomic_seq, 1);
    __sync_synchronize();
}

inline void SeqLock_WriteUnlock(struct SeqLock* sl) {
    __sync_synchronize();
    AtomicAdd(&sl->atomic_seq, 1);
    __sync_synchronize();
    SpinLock_Unlock(&sl->lock);
}
