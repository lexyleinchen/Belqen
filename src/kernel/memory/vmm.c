#include "vmm.h"
#include "pmm.h"
#include "address_space.h"
#include "../interrupts/interrupts.h"
#include "../core/log.h"

#define PAGE_MASK 0x000FFFFFFFFFF000ULL

static uint64_t kernel_directory;

static void load_directory(uint64_t directory) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(directory) : "memory");
}

static uint64_t* table_from_address(uint64_t address) {
    return (uint64_t*)(uintptr_t)address;
}

static uint64_t* get_table(uint64_t* parent, uint64_t index, uint64_t flags) {
    uint64_t entry = parent[index];

    if (!(entry & VMM_PRESENT)) {
        uint64_t page = pmm_allocate_page();

        if (page == 0) {
            return 0;
        }

        parent[index] = page | VMM_PRESENT | VMM_WRITABLE | (flags & VMM_USER);
        return (uint64_t*)(uintptr_t)page;
    }

    parent[index] |= VMM_WRITABLE | (flags & VMM_USER);
    return (uint64_t*)(uintptr_t)(parent[index] & VMM_ADDRESS_MASK);
}

void vmm_init(void) {
    uint64_t current_directory = vmm_get_current_directory();
    uint64_t new_directory = pmm_allocate_page();

    if (new_directory == 0) {
        return;
    }

    uint64_t* source = (uint64_t*)(uintptr_t)current_directory;
    uint64_t* destination = (uint64_t*)(uintptr_t)new_directory;

    for (uint64_t i = 0; i < 512; i++) {
        destination[i] = source[i];
    }

    kernel_directory = new_directory;
    load_directory(kernel_directory);
}

void vmm_test(void) {
    uint64_t test_physical = pmm_allocate_page();
    uint64_t test_virtual = 0x100000000ULL;

    if (test_physical == 0) {
        kernel_panic("vmm test physical allocation failed");
    }

    if (!vmm_map_page(test_virtual, test_physical, VMM_WRITABLE)) {
        kernel_panic("vmm test page table allocation failed");
    }

    volatile uint64_t* test_memory = (volatile uint64_t*)(uintptr_t)test_virtual;
    test_memory[0] = 0x123456789ABCDEF0ULL;

    if (test_memory[0] != 0x123456789ABCDEF0ULL) {
        kernel_panic("vmm test read/write failed");
    }

    if (vmm_get_physical_address(test_virtual) != test_physical) {
        kernel_panic("vmm translation failed");
    }

    vmm_unmap_page(test_virtual);
    pmm_free_page(test_physical);
    kernel_log("Vmm test passed.");
}

uint64_t vmm_get_current_directory(void) {
    uint64_t directory;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(directory));
    return directory & PAGE_MASK;
}

static int split_huge_page(uint64_t* page_directory, uint64_t index) {
    uint64_t old_entry = page_directory[index];

    if (!(old_entry & VMM_HUGE_PAGE)) {
        return 1;
    }

    uint64_t page_table_physical = pmm_allocate_page();

    if (page_table_physical == 0) {
        return 0;
    }

    uint64_t* page_table = (uint64_t*)(uintptr_t)page_table_physical;
    uint64_t base_address = old_entry & 0x000FFFFFFFE00000ULL;
    uint64_t flags = old_entry & 0xFFF;
    flags &= ~VMM_HUGE_PAGE;

    for (uint64_t i = 0; i < 512; i++) {
        page_table[i] = base_address + i * VMM_PAGE_SIZE | flags | VMM_PRESENT;
    }

    page_directory[index] = page_table_physical | VMM_PRESENT | VMM_WRITABLE | (old_entry & VMM_USER);
    return 1;
}

int vmm_map_page(uint64_t virtual_address, uint64_t physical_address, uint64_t flags) {
    uint64_t* pml4 = table_from_address(vmm_get_current_directory());
    uint64_t pml4_index = (virtual_address >> 39) & 0x1FF;
    uint64_t pdpt_index = (virtual_address >> 30) & 0x1FF;
    uint64_t pd_index = (virtual_address >> 21) & 0x1FF;
    uint64_t pt_index = (virtual_address >> 12) & 0x1FF;
    uint64_t* pdpt = get_table(pml4, pml4_index, flags);

    if (!pdpt) {
        return 0;
    }

    uint64_t* pd = get_table(pdpt, pdpt_index, flags);

    if (!pd) {
        return 0;
    }

    if (pd[pd_index] & VMM_HUGE_PAGE) {
        if (!split_huge_page(pd, pd_index)) {
            return 0;
        }
    }

    uint64_t* pt = get_table(pd, pd_index, flags);

    if (!pt) {
        return 0;
    }

    pt[pt_index] = (physical_address & PAGE_MASK) | (flags & 0xFFF) | (flags & VMM_NO_EXECUTE) | VMM_PRESENT;
    __asm__ volatile ("invlpg (%0)" : : "r"(virtual_address) : "memory");
    return 1;
}

void vmm_unmap_page(uint64_t virtual_address) {
    uint64_t* pml4 = table_from_address(vmm_get_current_directory());
    uint64_t pml4_index = (virtual_address >> 39) & 0x1FF;
    uint64_t pdpt_index = (virtual_address >> 30) & 0x1FF;
    uint64_t pd_index = (virtual_address >> 21) & 0x1FF;
    uint64_t pt_index = (virtual_address >> 12) & 0x1FF;
    
    if (!(pml4[pml4_index] & VMM_PRESENT)) {
        return;
    }

    uint64_t* pdpt = table_from_address(pml4[pml4_index] & PAGE_MASK);

    if (!(pdpt[pdpt_index] & VMM_PRESENT)) {
        return;
    }

    uint64_t* pd = table_from_address(pdpt[pdpt_index] & PAGE_MASK);

    if (!(pd[pd_index] & VMM_PRESENT)) {
        return;
    }

    uint64_t* pt = table_from_address(pd[pd_index] & PAGE_MASK);
    pt[pt_index] = 0;
    __asm__ volatile ("invlpg (%0)" : : "r"(virtual_address) : "memory");
}

uint64_t vmm_get_physical_address(uint64_t virtual_address) {
    uint64_t* pml4 = (uint64_t*)(uintptr_t)vmm_get_current_directory();
    uint64_t pml4_index = (virtual_address >> 39) & 0x1FF;
    uint64_t pdpt_index = (virtual_address >> 30) & 0x1FF;
    uint64_t pd_index = (virtual_address >> 21) & 0x1FF;
    uint64_t pt_index = (virtual_address >> 12) & 0x1FF;

    if (!(pml4[pml4_index] & VMM_PRESENT)) {
        return 0;
    }

    uint64_t* pdpt = (uint64_t*)(uintptr_t)(pml4[pml4_index] & VMM_ADDRESS_MASK);

    if (!(pdpt[pdpt_index] & VMM_PRESENT)) {
        return 0;
    }

    uint64_t* pd = (uint64_t*)(uintptr_t)(pdpt[pdpt_index] & VMM_ADDRESS_MASK);

    if (!(pd[pd_index] & VMM_PRESENT)) {
        return 0;
    }

    if (pd[pd_index] & VMM_HUGE_PAGE) {
        uint64_t base = pd[pd_index] & 0x000FFFFFFFE00000ULL;
        return base + (virtual_address & 0x1FFFFF);
    }

    uint64_t* pt = (uint64_t*)(uintptr_t)(pd[pd_index] & VMM_ADDRESS_MASK);

    if (!(pt[pt_index] & VMM_PRESENT)) {
        return 0;
    }

    return (pt[pt_index] & VMM_ADDRESS_MASK) | (virtual_address & 0xFFF);
}

void vmm_switch_directory(uint64_t directory) {
    if (directory == 0 || directory % VMM_PAGE_SIZE != 0) {
        return;
    }

    load_directory(directory);
}

int vmm_handle_page_fault(uint64_t address, uint64_t error_code) {
    AddressSpace* space = address_space_current();

    if (!space) {
        return 0;
    }

    if (error_code & VMM_PRESENT) {
        return 0;
    }

    uint64_t region_flags = 0;
    uint64_t page_address = address & PAGE_MASK;

    if (!address_space_contains(space, page_address, &region_flags)) {
        return 0;
    }

    if ((error_code & VMM_WRITABLE) && !(region_flags & VMM_WRITABLE)) {
        return 0;
    }

    if ((error_code & VMM_INSTRUCTION_FETCH) && (region_flags & VMM_NO_EXECUTE)) {
        return 0;
    }

    if (vmm_get_physical_address(page_address)) {
        return 0;
    }

    uint64_t physical_address = pmm_allocate_page();

    if (!physical_address) {
        return 0;
    }

    uint64_t map_flags = region_flags | VMM_USER;

    if (!vmm_map_page(page_address, physical_address, map_flags)) {
        pmm_free_page(physical_address);
        return 0;
    }

    return 1;
}

uint64_t vmm_get_kernel_directory(void) {
    return kernel_directory;
}