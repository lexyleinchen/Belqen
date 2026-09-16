#ifndef MALLOC_H
#define MALLOC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void* malloc(uint64_t size);

void free(void* address);

#ifdef __cplusplus
}
#endif

#endif // MALLOC_H