#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void multiboot_init(uint32_t address);

const void* multiboot_get_rsdp(void);

uint32_t multiboot_get_address(void);

#ifdef __cplusplus
}
#endif

#endif // MULTIBOOT_H