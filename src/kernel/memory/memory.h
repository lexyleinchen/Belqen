#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
} MemoryRegion;

void memory_init(uint32_t multiboot_address);

#ifdef __cplusplus
}
#endif

#endif // MEMORY_H