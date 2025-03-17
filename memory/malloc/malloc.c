#include "malloc.h"

void* malloc(size_t size) {
    void* ptr = mmap(NULL, size + sizeof(size_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    *((size_t*) ptr) = size + sizeof(size_t);
    return (size_t*) ptr + 1;
}

void free(void* ptr) {
    const size_t allocated_size = *((size_t*) ptr - 1);
    munmap((size_t*) ptr - 1, allocated_size);
}

void* realloc(void* ptr, size_t size) {
    void* new_ptr = mremap((size_t*) ptr - 1, *((size_t*) ptr - 1), size + sizeof(size_t), MREMAP_MAYMOVE, NULL);
    if (new_ptr == MAP_FAILED) {
        return NULL;
    }
    *((size_t*) new_ptr) = size + sizeof(size_t);
    return (size_t*) new_ptr + 1;
}
