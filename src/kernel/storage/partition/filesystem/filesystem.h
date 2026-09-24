#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdint.h>

#include "../../block.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FILESYSTEM_UNKNOWN,
    FILESYSTEM_FAT32
} FilesystemType;

typedef struct {
    char name[256];
    uint8_t is_directory;
    uint64_t size;
    uint32_t cluster;
} FilesystemEntry;

typedef struct {
    BlockDevice* device;
    FilesystemType type;
    void* filesystem_data;
} Filesystem;

typedef struct {
    Filesystem* filesystem;
    uint32_t first_cluster;
    uint64_t size;
    uint64_t position;
    uint64_t directory_lba;
    uint32_t directory_offset;
    uint8_t is_directory;
} FilesystemFile;

typedef enum {
    FILESYSTEM_SEEK_SET,
    FILESYSTEM_SEEK_CURRENT,
    FILESYSTEM_SEEK_END
} FilesystemSeekWhence;

int filesystem_mount(BlockDevice* device, Filesystem* filesystem);

int filesystem_read_directory(Filesystem* filesystem, uint32_t cluster, FilesystemEntry* entries, uint32_t max_entries, uint32_t* entry_count);

uint32_t filesystem_get_count(void);

Filesystem* filesystem_get(uint32_t index);

int filesystem_create_directory(Filesystem* filesystem, uint32_t parent_cluster, const char* name);

int filesystem_create_file(Filesystem* filesystem, uint32_t parent_cluster, const char* name);

int filesystem_open(Filesystem* filesystem, const char* name, FilesystemFile* file);

int filesystem_read(FilesystemFile* file, void* buffer, uint32_t size, uint32_t* bytes_read);

int filesystem_write(FilesystemFile* file, const void* buffer, uint32_t size, uint32_t* bytes_written);

int filesystem_seek(FilesystemFile* file, int64_t offset, FilesystemSeekWhence whence);

int filesystem_close(FilesystemFile* file);

int filesystem_open_path(Filesystem* filesystem, const char* path, FilesystemFile* file);

int filesystem_create_directory_path(Filesystem* filesystem, const char* path);

int filesystem_create_file_path(Filesystem* filesystem, const char* path);

int filesystem_delete_directory(Filesystem* filesystem, const char* path);

int filesystem_delete_file(Filesystem* filesystem, const char* path);

#ifdef __cplusplus
}
#endif

#endif // FILESYSTEM_H