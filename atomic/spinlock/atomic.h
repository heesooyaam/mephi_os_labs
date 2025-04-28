#pragma once

#include <stdint.h>

inline void AtomicAdd(int64_t* atomic, int64_t value) {
    asm volatile(
        "lock addq %1, %0"
        : "+m" (*atomic)
        : "ir" (value)
        : "memory", "cc"
    );
}

inline void AtomicSub(int64_t* atomic, int64_t value) {
    asm volatile(
        "lock subq %1, %0"
        : "+m" (*atomic)
        : "ir" (value)
        : "memory", "cc"
    );
}

inline int64_t AtomicXchg(int64_t* atomic, int64_t value) {
    asm volatile(
        "lock xchgq %0, %1"
        : "+r" (value), "+m" (*atomic)
        :
        : "memory", "cc"
    );
    return value;
}

inline int64_t AtomicCas(int64_t* atomic, int64_t* expected, int64_t value) {
    int64_t expected_val = *expected;
    unsigned char ok;

    asm volatile(
        "lock cmpxchgq %3, %1\n\t"
        "sete %b0"
        : "=q" (ok),
          "+m" (*atomic),
          "+a" (expected_val)
        : "r"  (value)
        : "memory", "cc"
    );

    if (!ok) {
        *expected = expected_val;
    }

    return ok;
}