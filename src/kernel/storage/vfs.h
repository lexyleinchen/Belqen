#ifndef VFS_H
#define VFS_H

#include <stdint.h>

#include "partition/filesystem/filesystem.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VFS_MAX_MOUNTS 8
#define VFS_PATH_MAX 128

typedef struct {
    char mount_point[VFS_PATH_MAX];
    Filesystem* filesystem;
} VfsMount;

typedef struct {
    VfsMount mounts[VFS_MAX_MOUNTS];
    uint32_t mount_count;
} Vfs;

typedef struct {
    Filesystem* filesystem;
    uint32_t cluster;
    uint32_t position;
} VfsDirectory;

void vfs_init(void);

int vfs_mount_root(Filesystem* filesystem);

int vfs_mount(const char* mount_point, Filesystem* filesystem);

int vfs_resolve_path(const char* path, Filesystem** filesystem, char* relative_path, uint32_t relative_path_size);

int vfs_open_path(const char* path, FilesystemFile* file);

int vfs_create_directory_path(const char* path);

int vfs_create_file_path(const char* path);

int vfs_delete_directory(const char* path);

int vfs_delete_file(const char* path);

int vfs_read(FilesystemFile* file, void* buffer, uint32_t size, uint32_t* bytes_read);

int vfs_write(FilesystemFile* file, const void* buffer, uint32_t size, uint32_t* bytes_written);

int vfs_seek(FilesystemFile* file, int64_t offset, FilesystemSeekWhence whence);

int vfs_close(FilesystemFile* file);

int vfs_open_directory(const char* path, VfsDirectory* directory);

int vfs_read_directory(VfsDirectory* directory, FilesystemEntry* entry, uint8_t* has_entry);

int vfs_close_directory(VfsDirectory* directory);

#ifdef __cplusplus
}
#endif

#endif // VFS_H