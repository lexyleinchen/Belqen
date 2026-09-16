#include "pmm.h"
#include "../interrupts/interrupts.h"
#include "../core/log.h"

#define MULTIBOOT_TAG_TYPE_END 0
#define MULTIBOOT_TAG_TYPE_MMAP 6
#define MULTIBOOT_MEMORY_AVAILABLE 1

extern uint8_t __kernel_start[];
extern uint8_t __kernel_end[];
extern uint8_t stack_bottom[];
extern uint8_t stack_top[];

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
} MultibootMemoryMapTag;

typedef struct {
    uint64_t base_address;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
} MultibootMemoryMapEntry;

typedef struct {
    uint32_t type;
    uint32_t size;
} MultibootTag;

typedef struct {
    uint32_t total_size;
    uint32_t reserved;
} MultibootInfo;

static uint8_t* page_bitmap;
static uint64_t maximum_page;
static uint64_t total_page_count;
static uint64_t free_page_count;

static void bitmap_set(uint64_t page) {
    page_bitmap[page / 8] |= (uint8_t)(1 << (page % 8));
}

static void bitmap_clear(uint64_t page) {
    page_bitmap[page / 8] &= (uint8_t)~(1 << (page % 8));
}

static int bitmap_test(uint64_t page) {
    return page_bitmap[page / 8] & (uint8_t)(1 << (page % 8));
}

static void reserve_range(uint64_t address, uint64_t length) {
    uint64_t first_page = address / PMM_PAGE_SIZE;
    uint64_t last_page = (address + length + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;

    if (last_page > maximum_page) {
        last_page = maximum_page;
    }

    for (uint64_t page = first_page; page < last_page; page++) {
        if (!bitmap_test(page)) {
            bitmap_set(page);
            free_page_count--;
        }
    }
}

void pmm_init(uint32_t multiboot_address) {
    kernel_log("Initializing physical memory...");
    MultibootInfo* info = (MultibootInfo*)(uintptr_t)multiboot_address;
    uint32_t current = multiboot_address + 8;
    uint64_t multiboot_end = (uint64_t)multiboot_address + info->total_size;
    uint64_t memory_end = 0;

    while (current + sizeof(MultibootTag) <= multiboot_end) {
        MultibootTag* tag = (MultibootTag*)(uintptr_t)current;

        if (tag->type == MULTIBOOT_TAG_TYPE_END) {
            break;
        }

        if (tag->size < sizeof(MultibootTag) || (uint64_t)current + tag->size > multiboot_end) {
            kernel_log("Invalid multiboot tag size.");
            return;
        }

        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            MultibootMemoryMapTag* map = (MultibootMemoryMapTag*)tag;
            
            if (map->entry_size < sizeof(MultibootMemoryMapEntry) || map->size < sizeof(MultibootMemoryMapTag) || map->size > tag->size) {
                kernel_log("Invalid multiboot memory map.");
                return;
            }

            uint32_t offset = sizeof(MultibootMemoryMapTag);

            while (offset <= map->size - map->entry_size) {
                MultibootMemoryMapEntry* entry = (MultibootMemoryMapEntry*)((uint8_t*)map + offset);

                if (entry->type == MULTIBOOT_MEMORY_AVAILABLE && entry->length != 0) {
                    uint64_t start = entry->base_address;
                    uint64_t end = start + entry->length;

                    if (end < start) {
                        end = UINT64_MAX;
                    }

                    if (end > memory_end) {
                        memory_end = end;
                    }

                    uint64_t address = start;

                    while (address < end) {
                        uint64_t page = address / PMM_PAGE_SIZE;

                        if (page < maximum_page && bitmap_test(page)) {
                            bitmap_clear(page);
                            free_page_count++;
                        }

                        if (UINT64_MAX - address < PMM_PAGE_SIZE) {
                            break;
                        }

                        address += PMM_PAGE_SIZE;
                    }
                }

                offset += map->entry_size;
            }
        }

        if (tag->size > 0x100000) {
            kernel_log("Invalid multiboot tag size.");
            return;
        }

        current += (tag->size + 7) & ~7;
    }

    if (memory_end == 0) {
        kernel_panic("no usable memory map");
    }

    maximum_page = (memory_end + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
    total_page_count = maximum_page;
    free_page_count = 0;
    uintptr_t bitmap_base = (uintptr_t)__kernel_end;

    if (multiboot_end > bitmap_base) {
        bitmap_base = multiboot_end;
    }

    page_bitmap = (uint8_t*)((bitmap_base + PMM_PAGE_SIZE - 1) & ~(PMM_PAGE_SIZE - 1));

    for (uint64_t page = 0; page < (maximum_page + 7) / 8; page++) {
        page_bitmap[page] = 0xFF;
    }

    current = multiboot_address + 8;

    while (current + sizeof(MultibootTag) <= multiboot_end) {
        MultibootTag* tag = (MultibootTag*)(uintptr_t)current;

        if (tag->type == MULTIBOOT_TAG_TYPE_END) {
            break;
        }

        if (tag->size < sizeof(MultibootTag) || (uint64_t)current + tag->size > multiboot_end) {
            kernel_log("Invalid multiboot tag size.");
            return;
        }

        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            MultibootMemoryMapTag* map = (MultibootMemoryMapTag*)tag;
            
            if (map->entry_size < sizeof(MultibootMemoryMapEntry) || map->size < sizeof(MultibootMemoryMapTag) || map->size > tag->size) {
                kernel_log("Invalid multiboot memory map.");
                return;
            }

            uint32_t offset = sizeof(MultibootMemoryMapTag);

            while (offset <= map->size - map->entry_size) {
                MultibootMemoryMapEntry* entry = (MultibootMemoryMapEntry*)((uint8_t*)map + offset);

                if (entry->type == MULTIBOOT_MEMORY_AVAILABLE && entry->length != 0) {
                    uint64_t start = entry->base_address;
                    uint64_t end = start + entry->length;

                    if (end < start) {
                        end = UINT64_MAX;
                    }

                    uint64_t address = start;

                    while (address < end) {
                        uint64_t page = address / PMM_PAGE_SIZE;

                        if (page < maximum_page && bitmap_test(page)) {
                            bitmap_clear(page);
                            free_page_count++;
                        }

                        if (UINT64_MAX - address < PMM_PAGE_SIZE) {
                            break;
                        }

                        address += PMM_PAGE_SIZE;
                    }
                }

                offset += map->entry_size;
            }
        }

        if (tag->size > 0x100000) {
            kernel_log("Invalid multiboot tag size.");
            return;
        }

        current += (tag->size + 7) & ~7;
    }

    reserve_range(0, 0x100000);
    reserve_range((uint64_t)__kernel_start, (uint64_t)(__kernel_end - __kernel_start));
    reserve_range((uint64_t)stack_bottom, (uint64_t)(stack_top - stack_bottom));
    reserve_range((uint64_t)page_bitmap, (maximum_page + 7) / 8);
    reserve_range(multiboot_address, info->total_size);
    kernel_log("Physical memory initialized.");
}

uint64_t pmm_allocate_page(void) {
    for (uint64_t page = 0; page < maximum_page; page++) {
        if (!bitmap_test(page)) {
            bitmap_set(page);
            free_page_count--;
            uint64_t address = page * PMM_PAGE_SIZE;

            for (uint64_t i = 0; i < PMM_PAGE_SIZE; i++) {
                ((uint8_t*)(uintptr_t)address)[i] = 0;
            }

            return address;
        }
    }

    return 0;
}

void pmm_free_page(uint64_t address) {
    if (address == 0 || (address % PMM_PAGE_SIZE) != 0) {
        return;
    }

    uint64_t page = address / PMM_PAGE_SIZE;

    if (page < maximum_page && bitmap_test(page)) {
        bitmap_clear(page);
        free_page_count++;
    }
}

uint64_t pmm_total_pages(void) {
    return total_page_count;
}

uint64_t pmm_free_pages(void) {
    return free_page_count;
}