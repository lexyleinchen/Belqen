#include "block.h"

#define MAX_BLOCK_DEVICES 16

static BlockDevice* block_devices[MAX_BLOCK_DEVICES];
static uint32_t block_device_count = 0;

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

    return device->read(device, lba, count, buffer);
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

    return device->write(device, lba, count, buffer);
}

void block_register_device(BlockDevice* device) {
    if (!device) {
        return;
    } 

    if (block_device_count >= MAX_BLOCK_DEVICES) {
        return;
    }

    block_devices[block_device_count] = device;
    block_device_count++;
}

int block_unregister_device(BlockDevice* device) {
    if (!device) {
        return 0;
    }
    
    for (uint32_t i = 0; i < block_device_count; i++) {
        if (block_devices[i] != device) {
            continue;
        }

        for (uint32_t j = i; j + 1 < block_device_count; j++) {
            block_devices[j] = block_devices[j + 1];
        }

        block_device_count--;
        block_devices[block_device_count] = 0;
        return 1;
    }

    return 0;
}

uint32_t block_get_device_count(void) {
    return block_device_count;
}

BlockDevice* block_get_device(uint32_t index) {
    if (index >= block_device_count) {
        return 0;
    }

    return block_devices[index];
}