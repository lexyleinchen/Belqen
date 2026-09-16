#include "malloc.h"
#include "heap.h"

void* malloc(uint64_t size) {
    return kmalloc(size);
}

void free(void* address) {
    kfree(address);
}