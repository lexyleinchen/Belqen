#include "address_space.h"
#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "../interrupts/interrupts.h"
#include "../core/log.h"

static AddressSpace* current_space;

static uint64_t clone_table(uint64_t source_address, uint64_t level) {
    uint64_t destination_address = pmm_allocate_page();

    if (!destination_address) {
        return 0;
    }

    uint64_t* source = (uint64_t*)(uintptr_t)source_address;
    uint64_t* destination = (uint64_t*)(uintptr_t)destination_address;

    for (uint64_t i = 0; i < 512; i++) {
        uint64_t entry = source[i];

        if (!(entry & VMM_PRESENT)) {
            continue;
        }

        if (level == 1 || ((level == 2 || level == 3) && (entry & VMM_HUGE_PAGE))) {
            destination[i] = entry;
            continue;
        }

        uint64_t child_address = clone_table(entry & VMM_ADDRESS_MASK, level - 1);

        if (!child_address) {
            pmm_free_page(destination_address);
            return 0;
        }

        destination[i] = child_address | (entry & ~VMM_ADDRESS_MASK);
    }

    return destination_address;
}

static void destroy_table(uint64_t table_address, uint64_t level) {
    uint64_t* table = (uint64_t*)(uintptr_t)table_address;

    if (level > 1) {
        for (uint64_t i = 0; i < 512; i++) {
            uint64_t entry = table[i];

            if (!(entry & VMM_PRESENT)) {
                continue;
            }

            if ((level == 2 || level == 3) && (entry & VMM_HUGE_PAGE)) {
                continue;
            }

            destroy_table(entry & VMM_ADDRESS_MASK, level - 1);
        }
    }

    pmm_free_page(table_address);
}

AddressSpace* address_space_create(void) {
    AddressSpace* space = kmalloc(sizeof(AddressSpace));

    if (!space) {
        return 0;
    }

    space->directory = pmm_allocate_page();
    space->regions = 0;

    if (!space->directory) {
        kfree(space);
        return 0;
    }

    uint64_t current_directory = vmm_get_current_directory();
    uint64_t cloned_directory = clone_table(current_directory, 4);

    if (!cloned_directory) {
        kfree(space);
        return 0;
    }

    pmm_free_page(space->directory);
    space->directory = cloned_directory;
    return space;
}

void address_space_test(void) {
    AddressSpace* space = address_space_create();

    if (!space) {
        kernel_panic("address space creation failed");
    }

    uint64_t physical = pmm_allocate_page();

    if (!physical) {
        kernel_panic("address space physical allocation failed");
    }

    if (!address_space_add_region(space, USER_SPACE_START, VMM_PAGE_SIZE, VMM_WRITABLE)) {
        kernel_panic("address space region failed");
    }

    if (!address_space_map(space, USER_SPACE_START, physical, VMM_WRITABLE)) {
        kernel_panic("address space mapping failed");
    }

    address_space_switch(space);
    volatile uint64_t* memory = (volatile uint64_t*)(uintptr_t)USER_SPACE_START;
    *memory = 0x123456789ABCDEF0ULL;

    if (*memory != 0x123456789ABCDEF0ULL) {
        kernel_panic("address space read/write failed");
    }

    uint64_t lazy_address = USER_SPACE_START + VMM_PAGE_SIZE;

    if (!address_space_add_region(space, lazy_address, VMM_PAGE_SIZE, VMM_WRITABLE)) {
        kernel_panic("lazy region failed");
    }

    kernel_log("Triggering demand page fault.");

    volatile uint64_t* lazy_memory = (volatile uint64_t*)(uintptr_t)lazy_address;
    *lazy_memory = 0x123456789ABCDEF0ULL;

    if (*lazy_memory != 0x123456789ABCDEF0ULL) {
        kernel_panic("demand page fault test failed");
    }

    kernel_log("Demand page fault test passed.");
    address_space_unmap(space, USER_SPACE_START);
    address_space_unmap(space, lazy_address);
    address_space_switch(0);
    address_space_destroy(space);
    kernel_log("Address space test passed.");
}

void address_space_destroy(AddressSpace* space) {
    if (!space) {
        return;
    }

    uint64_t old_directory = vmm_get_current_directory();
    int was_current = current_space == space;
    AddressRegion* region = space->regions;

    while (region) {
        for (uint64_t address = region->start; address < region->end; address += VMM_PAGE_SIZE) {
            uint64_t physical = vmm_get_physical_address(address);

            if (physical) {
                vmm_unmap_page(address);
                pmm_free_page(physical);
            }
        }

        AddressRegion* next = region->next;
        kfree(region);
        region = next;
    }

    if (was_current) {
        vmm_switch_directory(vmm_get_kernel_directory());
        current_space = 0;
    }
    else {
        vmm_switch_directory(old_directory);
    }

    destroy_table(space->directory, 4);
    kfree(space);
}

void address_space_switch(AddressSpace* space) {
    if (space) {
        vmm_switch_directory(space->directory);
        current_space = space;
        return;
    }

    vmm_switch_directory(vmm_get_kernel_directory());
    current_space = 0;
}

int address_space_map(AddressSpace* space, uint64_t virtual_address, uint64_t physical_address, uint64_t flags) {
    if (!space || virtual_address < USER_SPACE_START || virtual_address >= USER_SPACE_END || (virtual_address % VMM_PAGE_SIZE) != 0 || (physical_address % VMM_PAGE_SIZE) != 0) {
        return 0;
    }

    uint64_t region_flags = 0;

    if (!address_space_contains(space, virtual_address, &region_flags)) {
        return 0;
    }

    if ((flags & VMM_WRITABLE) && !(region_flags & VMM_WRITABLE)) {
        return 0;
    }

    uint64_t old_directory = vmm_get_current_directory();
    vmm_switch_directory(space->directory);

    if (vmm_get_physical_address(virtual_address)) {
        vmm_switch_directory(old_directory);
        return 0;
    }

    int result = vmm_map_page(virtual_address, physical_address, flags | VMM_USER);
    vmm_switch_directory(old_directory);
    return result;
}

int address_space_add_region(AddressSpace* space, uint64_t start, uint64_t size, uint64_t flags) {
    if (!space || size == 0) {
        return 0;
    }

    if ((start % VMM_PAGE_SIZE) != 0 || (size % VMM_PAGE_SIZE) != 0) {
        return 0;
    }

    if (start < USER_SPACE_START || start + size < start || start + size > USER_SPACE_END) {
        return 0;
    }

    AddressRegion* existing = space->regions;

    while (existing) {
        if (start < existing->end && start + size > existing->start) {
            return 0;
        }

        existing = existing->next;
    }

    AddressRegion* region = kmalloc(sizeof(AddressRegion));

    if (!region) {
        return 0;
    }

    region->start = start;
    region->end = start + size;
    region->flags = flags;
    region->next = space->regions;
    space->regions = region;
    return 1;
}

int address_space_contains(AddressSpace* space, uint64_t address, uint64_t* flags) {
    if (!space) {
        return 0;
    }

    AddressRegion* region = space->regions;

    while (region) {
        if (address >= region->start && address < region->end) {
            if (flags) {
                *flags = region->flags;
            }

            return 1;
        }

        region = region->next;
    }

    return 0;
}

int address_space_unmap(AddressSpace* space, uint64_t virtual_address) {
    if (!space || virtual_address < USER_SPACE_START || virtual_address >= USER_SPACE_END || (virtual_address % VMM_PAGE_SIZE) != 0) {
        return 0;
    }

    uint64_t old_directory = vmm_get_current_directory();
    vmm_switch_directory(space->directory);
    uint64_t physical_address = vmm_get_physical_address(virtual_address);

    if (!physical_address) {
        vmm_switch_directory(old_directory);
        return 0;
    }

    vmm_unmap_page(virtual_address);
    pmm_free_page(physical_address);
    vmm_switch_directory(old_directory);
    return 1;
}

AddressSpace* address_space_current(void) {
    return current_space;
}