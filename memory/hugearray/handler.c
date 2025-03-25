#include <stdlib.h>
#include <unistd.h>
#include <bits/types/siginfo_t.h>
#include <sys/mman.h>

extern size_t PAGE_SIZE;
extern double* SQRTS;

void CalculateSqrts(double* sqrt_pos, int start, int n);

void HandleSigsegv(int sig, siginfo_t* siginfo, void* ctx) {
    static void* ptr = NULL;

    if (ptr) {
        munmap(ptr, PAGE_SIZE);
    }

    double* pg_start = (double*) ((size_t) siginfo->si_addr & ~(PAGE_SIZE - 1));
    ptr = mmap(pg_start, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);

    CalculateSqrts(ptr, (double*) ptr - SQRTS, PAGE_SIZE / sizeof(double));
}
