#ifndef VMM_H
#define VMM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VMM_PRESENT 0x001
#define VMM_WRITABLE 0x002
#define VMM_USER 0x004
#define VMM_NO_EXECUTE (1ULL << 63)
#define VMM_PAGE_SIZE 4096
#define VMM_HUGE_PAGE 0x080
#define VMM_ADDRESS_MASK 0x000FFFFFFFFFF000ULL
#define VMM_INSTRUCTION_FETCH (1ULL << 4)

void vmm_init(void);

void vmm_test(void);

uint64_t vmm_get_current_directory(void);

int vmm_map_page(uint64_t virtual_address, uint64_t physical_address, uint64_t flags);

void vmm_unmap_page(uint64_t virtual_address);

uint64_t vmm_get_physical_address(uint64_t virtual_address);

void vmm_switch_directory(uint64_t directory);

int vmm_handle_page_fault(uint64_t address, uint64_t error_code);

uint64_t vmm_get_kernel_directory(void);

#ifdef __cplusplus
}
#endif

#endif // VMM_H