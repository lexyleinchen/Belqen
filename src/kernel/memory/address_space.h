#ifndef ADDRESS_SPACE_H
#define ADDRESS_SPACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define USER_SPACE_START 0x0000000300000000ULL
#define USER_SPACE_END 0x0000800000000000ULL

typedef struct AddressRegion {
    uint64_t start;
    uint64_t end;
    uint64_t flags;
    struct AddressRegion* next;
} AddressRegion;

typedef struct AddressSpace {
    uint64_t directory;
    AddressRegion* regions;
} AddressSpace;

AddressSpace* address_space_create(void);

void address_space_test(void);

void address_space_destroy(AddressSpace* space);

void address_space_switch(AddressSpace* space);

int address_space_map(AddressSpace* space, uint64_t virtual_address, uint64_t physical_address, uint64_t flags);

int address_space_add_region(AddressSpace* space, uint64_t start, uint64_t size, uint64_t flags);

int address_space_contains(AddressSpace* space, uint64_t address, uint64_t* flags);

int address_space_unmap(AddressSpace* space, uint64_t virtual_address);

int address_space_validate_user_buffer(AddressSpace* space, uint64_t address, uint64_t size, uint64_t required_flags);

AddressSpace* address_space_current(void);

#ifdef __cplusplus
}
#endif

#endif // ADDRESS_SPACE_H