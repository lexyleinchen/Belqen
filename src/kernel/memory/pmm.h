#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PMM_PAGE_SIZE 4096

void pmm_init(uint32_t multiboot_address);

uint64_t pmm_allocate_page(void);

void pmm_free_page(uint64_t address);

uint64_t pmm_total_pages(void);

uint64_t pmm_free_pages(void);

#ifdef __cplusplus
}
#endif

#endif // PMM_H