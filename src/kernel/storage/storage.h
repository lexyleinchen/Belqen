#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>

#include "partition/filesystem/filesystem.h"

#ifdef __cplusplus
extern "C" {
#endif

void storage_init(void);

uint32_t storage_get_filesystem_count(void);

Filesystem* storage_get_filesystem(uint32_t index);

int storage_mount_partition(BlockDevice* partition);

int storage_unmount_partition(BlockDevice* partition);

int storage_mount_root(Filesystem* filesystem);

#ifdef __cplusplus
}
#endif

#endif // STORAGE_H