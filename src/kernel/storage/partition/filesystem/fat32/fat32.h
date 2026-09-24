#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>

#include "../filesystem.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t fat_count;
    uint32_t sectors_per_fat;
    uint32_t root_cluster;
    uint32_t fat_start_lba;
    uint32_t data_start_lba;
    uint32_t cluster_count;
} FAT32Filesystem;

typedef struct {
    BlockDevice* device;
    uint32_t stage;
    uint32_t progress;
    uint32_t fat_sectors;
    uint32_t fat_index;
    uint32_t total_sectors;
    uint32_t sectors_per_cluster;
    uint32_t reserved_sector_count;
    uint32_t fat_count;
    uint32_t root_lba;
    uint8_t sector[512];
    int work_id;
    uint32_t total_progress;
} FAT32FormatWork;

typedef enum {
    FAT32_FORMAT_IDLE,
    FAT32_FORMAT_RUNNING,
    FAT32_FORMAT_COMPLETE,
    FAT32_FORMAT_FAILED
} FAT32FormatStatus;

int fat32_mount(BlockDevice* device, Filesystem* filesystem);

int fat32_format(BlockDevice* device);

int fat32_format_async(BlockDevice* device, FAT32FormatWork* work);

FAT32FormatStatus fat32_format_get_status(FAT32FormatWork* work);

uint32_t fat32_format_get_progress(FAT32FormatWork* work);

uint32_t fat32_format_get_percent(FAT32FormatWork* work);

int fat32_create_directory(Filesystem* filesystem, uint32_t parent_cluster, const char* name);

int fat32_read_directory(Filesystem* filesystem, uint32_t cluster, FilesystemEntry* entries, uint32_t max_entries, uint32_t* entry_count);

int fat32_create_file(Filesystem* filesystem, uint32_t parent_cluster, const char* name);

int fat32_open_file(Filesystem* filesystem, const char* name, FilesystemFile* file);

int fat32_read_file(FilesystemFile* file, void* buffer, uint32_t size, uint32_t* bytes_read);

int fat32_write_file(FilesystemFile* file, const void* buffer, uint32_t size, uint32_t* bytes_written);

int fat32_open_path(Filesystem* filesystem, const char* path, FilesystemFile* file);

int fat32_create_directory_path(Filesystem* filesystem, const char* path);

int fat32_create_file_path(Filesystem* filesystem, const char* path);

int fat32_delete_directory(Filesystem* filesystem, const char* path);

int fat32_delete_file(Filesystem* filesystem, const char* path);

#ifdef __cplusplus
}
#endif

#endif // FAT32_H