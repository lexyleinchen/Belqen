#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void heap_init(void);

void heap_test(void);

void* kmalloc(uint64_t size);

void kfree(void* address);

#ifdef __cplusplus
}
#endif

#endif // HEAP_H