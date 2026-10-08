#include "block.h"

#define MAX_BLOCK_DEVICES 16
#define BLOCK_CACHE_ENTRY_COUNT 16
#define BLOCK_CACHE_SECTOR_SIZE 512

typedef struct {
    BlockDevice* device;
    uint64_t lba;
    uint8_t valid;
    uint8_t dirty;
    uint8_t data[BLOCK_CACHE_SECTOR_SIZE];
} BlockCacheEntry;

static BlockDevice* block_devices[MAX_BLOCK_DEVICES];
static uint32_t block_device_count = 0;
static BlockCacheEntry block_cache[BLOCK_CACHE_ENTRY_COUNT];
static uint32_t block_cache_next_victim;
static volatile uint8_t block_cache_lock;


static void block_cache_acquire(void) {
    while (__atomic_test_and_set(&block_cache_lock, __ATOMIC_ACQUIRE)) {
        __asm__  volatile ("pause");
    }
}

static void block_cache_release(void) {
    __atomic_clear(&block_cache_lock, __ATOMIC_RELEASE);
}

static void block_cache_copy(uint8_t* destination, const uint8_t* source) {
    for (uint32_t i = 0; i < BLOCK_CACHE_SECTOR_SIZE; i++) {
        destination[i] = source[i];
    }
}

static BlockCacheEntry* block_cache_find_locked(BlockDevice* device, uint64_t lba) {
    for (uint32_t i = 0; i < BLOCK_CACHE_ENTRY_COUNT; i++) {
        BlockCacheEntry* entry =&block_cache[i];

        if (entry->valid && entry->device == device && entry->lba == lba) {
            return entry;
        }
    }

    return 0;
}

static int block_cache_flush_entry_locked(BlockCacheEntry* entry) {
    if (!entry->valid || !entry->dirty) {
        return 1;
    }

    if (!entry->device || !entry->device->write || !entry->device->write(entry->device, entry->lba, 1, entry->data)) {
        return 0;
    }

    entry->dirty = 0;
    return 1;
}

static BlockCacheEntry* block_cache_get_slot_locked(BlockDevice* device, uint64_t lba) {
    BlockCacheEntry* entry = block_cache_find_locked(device, lba);

    if (entry) {
        return entry;
    }

    entry = &block_cache[block_cache_next_victim];

    if (!block_cache_flush_entry_locked(entry)){
        return 0;
    }

    block_cache_next_victim = (block_cache_next_victim + 1) % BLOCK_CACHE_ENTRY_COUNT;
    entry->valid = 0;
    entry->dirty = 0;
    entry->device = device;
    entry->lba = lba;
    return entry;
}

static int block_cache_read_sector_locked(BlockDevice* device, uint64_t lba, void* buffer) {
    BlockCacheEntry* entry = block_cache_find_locked(device, lba);

    if (entry) {
        block_cache_copy((uint8_t*)buffer, entry->data);
        return 1;
    }

    entry = block_cache_get_slot_locked(device, lba);

    if (!entry || !device->read || !device->read(device, lba, 1, entry->data)) {
        if (entry) {
            entry->valid = 0;
        }

        return 0;
    }

    entry->valid = 1;
    entry->dirty = 0;
    block_cache_copy((uint8_t*)buffer, entry->data);
    return 1;
}

static int block_cache_write_sector_locked(BlockDevice* device, uint64_t lba, const void* buffer) {
    BlockCacheEntry* entry = block_cache_get_slot_locked(device, lba);

    if (!entry) {
        return 0;
    }

    block_cache_copy(entry->data, (const uint8_t*)buffer);
    entry->device = device;
    entry->lba = lba;
    entry->valid = 1;
    entry->dirty = 1;
    return 1;
}

int block_cache_flush(BlockDevice* device) {
    block_cache_acquire();

    for (uint32_t i = 0; i < BLOCK_CACHE_ENTRY_COUNT; i++) {
        BlockCacheEntry* entry = &block_cache[i];

        if (!entry->valid || (device && entry->device != device)) {
            continue;
        }

        if (!block_cache_flush_entry_locked(entry)) {
            block_cache_release();
            return 0;
        }
    }

    block_cache_release();
    return 1;
}

int block_cache_invalidate_device(BlockDevice* device) {
    if (!device) {
        return 0;
    }

    block_cache_acquire();

    for (uint32_t i = 0; i < BLOCK_CACHE_ENTRY_COUNT; i++) {
        BlockCacheEntry* entry = &block_cache[i];

        if (entry->valid && entry->device == device && !block_cache_flush_entry_locked(entry)) {
            block_cache_release();
            return 0;
        }
    }

    for (uint32_t i = 0; i < BLOCK_CACHE_ENTRY_COUNT; i++) {
        if (block_cache[i].device == device) {
            block_cache[i].valid = 0;
            block_cache[i].dirty = 0;
        }
    }

    block_cache_release();
    return 1;
}

int block_read(BlockDevice* device, uint64_t lba, uint32_t count, void* buffer) {
    if (!device || !device->read || !buffer) {
        return 0;
    }

    if (count == 0) {
        return 1;
    }

    if (lba >= device->sector_count || count > device->sector_count - lba) {
        return 0;
    }

    if (device->type != BLOCK_DEVICE_PHYSICAL || device->sector_size != BLOCK_CACHE_SECTOR_SIZE) {
        return device->read(device, lba, count, buffer);
    }

    block_cache_acquire();
    uint8_t* output = (uint8_t*)buffer;

    for (uint32_t i = 0; i < count; i++) {
        if (!block_cache_read_sector_locked(device, lba + i, &output[i * BLOCK_CACHE_SECTOR_SIZE])) {
            block_cache_release();
            return 0;
        }
    }

    block_cache_release();
    return 1;
}

int block_write(BlockDevice* device, uint64_t lba, uint32_t count, const void* buffer) {
    if (!device || !device->write || !buffer) {
        return 0;
    }

    if (count == 0) {
        return 1;
    }

    if (lba >= device->sector_count || count > device->sector_count - lba) {
        return 0;
    }

    if (device->type != BLOCK_DEVICE_PHYSICAL || device->sector_size != BLOCK_CACHE_SECTOR_SIZE) {
        return device->write(device, lba, count, buffer);
    }

    block_cache_acquire();
    const uint8_t* input = (const uint8_t*)buffer;

    for (uint32_t i = 0; i < count; i++) {
        if (!block_cache_write_sector_locked(device, lba + i, &input[i * BLOCK_CACHE_SECTOR_SIZE])) {
            block_cache_release();
            return 0;
        }
    }

    block_cache_release();
    return 1;
}

void block_register_device(BlockDevice* device) {
    if (!device) {
        return;
    } 

    if (block_device_count >= MAX_BLOCK_DEVICES) {
        return;
    }

    block_devices[block_device_count++] = device;
}

int block_unregister_device(BlockDevice* device) {
    if (!device) {
        return 0;
    }
    
    for (uint32_t i = 0; i < block_device_count; i++) {
        if (block_devices[i] != device) {
            continue;
        }

        if (!block_cache_invalidate_device(device)) {
            return 0;
        }

        for (uint32_t j = i; j + 1 < block_device_count; j++) {
            block_devices[j] = block_devices[j + 1];
        }

        block_devices[--block_device_count] = 0;
        return 1;
    }

    return 0;
}

uint32_t block_get_device_count(void) {
    return block_device_count;
}

BlockDevice* block_get_device(uint32_t index) {
    return index < block_device_count ? block_devices[index] : 0;
}