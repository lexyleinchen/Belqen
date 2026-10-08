#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BlockDevice BlockDevice;

typedef int (*BlockReadFunc) (
    BlockDevice* device,
    uint64_t lba,
    uint32_t count,
    void* buffer
);

typedef int (*BlockWriteFunc) (
    BlockDevice* device,
    uint64_t lba,
    uint32_t count,
    const void* buffer
);

typedef enum {
    BLOCK_DEVICE_PHYSICAL,
    BLOCK_DEVICE_PARTITION
} BlockDeviceType;

struct BlockDevice {
    uint64_t sector_count;
    uint32_t sector_size;
    BlockDeviceType type;
    BlockReadFunc read;
    BlockWriteFunc write;
    void* driver_data;
};

int block_cache_flush(BlockDevice* device);

int block_cache_invalidate_device(BlockDevice* device);

int block_read(BlockDevice* device, uint64_t lba, uint32_t count, void* buffer);

int block_write(BlockDevice* device, uint64_t lba, uint32_t count, const void* buffer);

void block_register_device(BlockDevice* device);

int block_unregister_device(BlockDevice* device);

uint32_t block_get_device_count(void);

BlockDevice* block_get_device(uint32_t index);

#ifdef __cplusplus
}
#endif

#endif // BLOCK_H