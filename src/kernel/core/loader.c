#include "loader.h"
#include "bpe.h"
#include "../memory/address_space.h"
#include "../memory/pmm.h"
#include "../memory/vmm.h"

#define BPE_LOAD_BASE USER_SPACE_START
#define BPE_CODE_START (BPE_LOAD_BASE + VMM_PAGE_SIZE)

static int range_valid(uint64_t offset, uint64_t size, uint64_t image_size) {
    if (offset > image_size) {
        return 0;
    }

    return size <= image_size - offset;
}

static uint64_t align_up(uint64_t value) {
    return (value + VMM_PAGE_SIZE - 1) & ~(VMM_PAGE_SIZE - 1);
}

static void copy_bytes(uint8_t* destination, const uint8_t* source, uint64_t count) {
    for (uint64_t i = 0; i < count; i++) {
        destination[i] = source[i];
    }
}

static void clear_bytes(uint8_t* destination, uint64_t size) {
    for (uint64_t i = 0; i < size; i++) {
        destination[i] = 0;
    }
}

static int map_region(AddressSpace* address_space, uint64_t virtual_address, uint64_t size, uint64_t flags) {
    if (size == 0) {
        return 1;
    }

    uint64_t region_start = virtual_address & ~(VMM_PAGE_SIZE - 1);
    uint64_t region_end = align_up(virtual_address + size);

    if (virtual_address + size < virtual_address) {
        return 0;
    }

    if (region_start < USER_SPACE_START || region_end > USER_SPACE_END || region_end <= region_start) {
        return 0;
    }

    if (!address_space_add_region(address_space, region_start, region_end - region_start, flags)) {
        return 0;
    }

    for (uint64_t page = region_start; page < region_end; page += VMM_PAGE_SIZE) {
        uint64_t physical = pmm_allocate_page();

        if (!physical) {
            return 0;
        }

        if (!address_space_map(address_space, page, physical, flags)) {
            pmm_free_page(physical);
            return 0;
        }
    }

    return 1;
}

static int bpe_header_valid(const BpeHeader* header, uint64_t image_size) {
    if (!header || image_size < sizeof(BpeHeader)) {
        return 0;
    }

    if (header->magic[0] != BPE_MAGIC0 || header->magic[1] != BPE_MAGIC1 || header->magic[2] != BPE_MAGIC2 || header->magic[3] != BPE_MAGIC3) {
        return 0;
    }

    if (header->version != BPE_VERSION || header->machine != BPE_MACHINE_X86_64 || header->header_size != sizeof(BpeHeader)) {
        return 0;
    }

    if (header->image_size != image_size || header->image_size < header->header_size) {
        return 0;
    }

    if (!range_valid(header->code_offset, header->code_size, image_size)) {
        return 0;
    }

    if (!range_valid(header->data_offset, header->data_size, image_size)) {
        return 0;
    }

    if (header->entry_offset < BPE_CODE_START || header->entry_offset >= BPE_CODE_START + header->code_size) {
        return 0;
    }

    if (header->code_size == 0 || !(header->flags & BPE_FLAG_CODE_EXECUTABLE)) {
        return 0;
    }

    if (header->stack_pages == 0) {
        return 0;
    }

    return 1;
}

int process_load_bpe(const void* image, uint64_t image_size, const char* name, Process** process_result, Thread** thread_result) {
    if (!image || !name || !process_result || !thread_result) {
        return 0;
    }

    const BpeHeader* header = (const BpeHeader*)image;

    if (!bpe_header_valid(header, image_size)) {
        return 0;
    }

    uint64_t code_start = BPE_CODE_START;
    uint64_t data_start = align_up(code_start + header->code_size);
    uint64_t bss_start = align_up(data_start + header->data_size);
    uint64_t bss_end = bss_start + header->bss_size;

    if (data_start < code_start || bss_start < data_start || bss_end < bss_start || bss_end > USER_SPACE_END) {
        return 0;
    }

    Process* process = process_create_user(name);

    if (!process) {
        return 0;
    }

    AddressSpace* address_space = (AddressSpace*)process->address_space;
    uint64_t code_flags = 0;
    
    if (!(header->flags & BPE_FLAG_CODE_EXECUTABLE)) {
        code_flags |= VMM_NO_EXECUTE;
    }

    if (!map_region(address_space, code_start, header->code_size, code_flags)) {
        process_exit(process, 1);
        return 0;
    }

    uint64_t data_flags = VMM_WRITABLE | VMM_NO_EXECUTE;

    if (header->data_size > 0 && !map_region(address_space, data_start, header->data_size, data_flags)) {
        process_exit(process, 1);
        return 0;
    }

    if (header->bss_size > 0 && !map_region(address_space, bss_start, header->bss_size, data_flags)) {
        process_exit(process, 1);
        return 0;
    }

    uint64_t old_directory = vmm_get_current_directory();
    vmm_switch_directory(address_space->directory);
    copy_bytes((uint8_t*)(uintptr_t)code_start, (const uint8_t*)image + header->code_offset, header->code_size);

    if (header->data_size > 0) {
        copy_bytes((uint8_t*)(uintptr_t)data_start, (const uint8_t*)image + header->data_offset, header->data_size);
    }

    if (header->bss_size > 0) {
        clear_bytes((uint8_t*)(uintptr_t)bss_start, header->bss_size);
    }

    vmm_switch_directory(old_directory);
    Thread* thread = thread_create_user(process, name, (void*)(uintptr_t)header->entry_offset, 1);

    if (!thread) {
        process_exit(process, 1);
        return 0;
    }

    process_set_state(process, PROCESS_STATE_READY);
    *process_result = process;
    *thread_result = thread;
    return 1;
}