#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "../interrupts/interrupts.h"
#include "../core/log.h"

#define KERNEL_HEAP_START 0x100000000ULL
#define KERNEL_HEAP_LIMIT 0x200000000ULL
#define HEAP_BLOCK_MAGIC 0x48454150424C4F43ULL

typedef struct HeapBlock {
    uint64_t size;
    uint64_t magic;
    int free;
    struct HeapBlock* next;
} HeapBlock;

static HeapBlock* heap_head;
static uint64_t heap_end;

static uint64_t align_size(uint64_t size) {
    return (size + 15) & ~15ULL;
}

static HeapBlock* expand_heap(uint64_t size) {
    uint64_t total_size = sizeof(HeapBlock) + size;
    uint64_t page_count = (total_size + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
    uint64_t start = heap_end;

    if (heap_end + page_count * PMM_PAGE_SIZE > KERNEL_HEAP_LIMIT) {
        return 0;
    }

    uint64_t mapped_pages = 0;

    for (uint64_t page = 0; page < page_count; page++) {
        uint64_t physical = pmm_allocate_page();

        if (physical == 0 || !vmm_map_page(heap_end, physical, VMM_WRITABLE)) {
            if (physical != 0) {
                pmm_free_page(physical);
            }

            while (mapped_pages > 0) {
                heap_end -= PMM_PAGE_SIZE;
                uint64_t mapped_physical = vmm_get_physical_address(heap_end);
                vmm_unmap_page(heap_end);

                if (mapped_physical != 0) {
                    pmm_free_page(mapped_physical);
                }

                mapped_pages--;
            }

            return 0;
        }

        heap_end += PMM_PAGE_SIZE;
        mapped_pages++;
    }

    HeapBlock* block = (HeapBlock*)(uintptr_t)start;
    block->size = page_count * PMM_PAGE_SIZE - sizeof(HeapBlock);
    block->magic = HEAP_BLOCK_MAGIC;
    block->free = 1;
    block->next = 0;
    return block;
}

void heap_init(void) {
    heap_end = KERNEL_HEAP_START;
    heap_head = 0;
}

void heap_test(void) {
    void* first = kmalloc(100);
    void* second = kmalloc(1000);

    if (!first || !second) {
        kernel_panic("heap allocation failed");
    }

    kfree(first);
    void* reused = kmalloc(64);

    if (reused != first) {
        kernel_panic("heap block reuse failed");
    }

    kfree(second);
    kfree(reused);
    kernel_log("Heap test passed.");
}

static void split_block(HeapBlock* block, uint64_t size) {
    uint64_t remaining = block->size - size;

    if (remaining < sizeof(HeapBlock) + 16) {
        return;
    }

    HeapBlock* new_block = (HeapBlock*)((uint8_t*)(block + 1) + size);
    new_block->size = remaining - sizeof(HeapBlock);
    new_block->magic = HEAP_BLOCK_MAGIC;
    new_block->free = 1;
    new_block->next = block->next;
    block->size = size;
    block->next = new_block;
}

void* kmalloc(uint64_t size) {
    if (size == 0) {
        return 0;
    }

    size = align_size(size);
    HeapBlock* block = heap_head;

    while(block) {
        if (block->free && block->size >= size) {
            split_block(block, size);
            block->free = 0;
            return (void*)(block + 1);
        }

        block = block->next;
    }

    HeapBlock* new_block = expand_heap(size);

    if (!new_block) {
        return 0;
    }

    new_block->free = 0;

    if (!heap_head || (uintptr_t)new_block < (uintptr_t)heap_head) {
        new_block->next = heap_head;
        heap_head = new_block;
    }
    else {
        HeapBlock* block = heap_head;

        while (block->next && (uintptr_t)block->next < (uintptr_t)new_block) {
            block = block->next;
        }

        new_block->next = block->next;
        block->next = new_block;
    }
    
    return (void*)(new_block + 1);
}

static void coalesce_free_blocks(void) {
    HeapBlock* block = heap_head;

    while (block && block->next) {
        HeapBlock* next = block->next;
        uint8_t* block_end = (uint8_t*)(block + 1) + block->size;

        if (block->free && next->free && (uint8_t*)next == block_end) {
            block->size += sizeof(HeapBlock) + next->size;
            block->next = next->next;
            continue;
        }

        block = next;
    }
}

void kfree(void* address) {
    if (!address || (uintptr_t) address < KERNEL_HEAP_START || (uintptr_t)address >= heap_end) {
        return;
    }

    HeapBlock* block = ((HeapBlock*)address) - 1;

    if (block->magic != HEAP_BLOCK_MAGIC || block->free) {
        return;
    }

    block->free = 1;
    coalesce_free_blocks();
}