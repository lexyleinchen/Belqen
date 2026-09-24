#include "filesystem.h"
#include "fat32/fat32.h"

#define MAX_MOUNTED_FILESYSTEMS 16

static FAT32Filesystem fat32_instances[MAX_MOUNTED_FILESYSTEMS];
static Filesystem* mounted_filesystems[MAX_MOUNTED_FILESYSTEMS];
static uint32_t mounted_filesystem_count = 0;

int filesystem_mount(BlockDevice* device, Filesystem* filesystem) {
    if (!device || !filesystem) {
        return 0;
    }

    if (mounted_filesystem_count >= MAX_MOUNTED_FILESYSTEMS) {
        return 0;
    }

    FAT32Filesystem* fat32 = &fat32_instances[mounted_filesystem_count];
    filesystem->device = device;
    filesystem->type = FILESYSTEM_UNKNOWN;
    filesystem->filesystem_data = fat32;

    if (fat32_mount(device, filesystem)) {
        mounted_filesystems[mounted_filesystem_count] = filesystem;
        mounted_filesystem_count++;
        return 1;
    }

    filesystem->device = 0;
    filesystem->type = FILESYSTEM_UNKNOWN;
    filesystem->filesystem_data = 0;
    return 0;
}

uint32_t filesystem_get_count(void) {
    return mounted_filesystem_count;
}

Filesystem* filesystem_get(uint32_t index) {
    if (index >= mounted_filesystem_count) {
        return 0;
    }

    return mounted_filesystems[index];
}

int filesystem_read_directory(Filesystem* filesystem, uint32_t cluster, FilesystemEntry* entries, uint32_t max_entries, uint32_t* entry_count) {
    if (!filesystem || !entries || !entry_count) {
        return 0;
    }

    *entry_count = 0;

    if (filesystem->type == FILESYSTEM_FAT32) {
        return fat32_read_directory(filesystem, cluster, entries, max_entries, entry_count);
    }

    return 0;
}

int filesystem_create_directory(Filesystem* filesystem, uint32_t parent_cluster, const char* name) {
    if (!filesystem || !name) {
        return 0;
    }

    if (filesystem->type == FILESYSTEM_FAT32) {
        return fat32_create_directory(filesystem, parent_cluster, name);
    }

    return 0;
}

int filesystem_create_file(Filesystem* filesystem, uint32_t parent_cluster, const char* name) {
    if (!filesystem || !name) {
        return 0;
    }

    if (filesystem->type == FILESYSTEM_FAT32) {
        return fat32_create_file(filesystem, parent_cluster, name);
    }

    return 0;
}

int filesystem_open(Filesystem* filesystem, const char* name, FilesystemFile* file) {
    if (!filesystem || !name || !file) {
        return 0;
    }

    if (filesystem->type == FILESYSTEM_FAT32) {
        return fat32_open_file(filesystem, name, file);
    }

    return 0;
}

int filesystem_read(FilesystemFile* file, void* buffer, uint32_t size, uint32_t* bytes_read) {
    if (!file || !buffer || !bytes_read) {
        return 0;
    }

    if (!file->filesystem) {
        return 0;
    }

    if (file->filesystem->type == FILESYSTEM_FAT32) {
        return fat32_read_file(file, buffer, size, bytes_read);
    }

    return 0;
}

int filesystem_write(FilesystemFile* file, const void* buffer, uint32_t size, uint32_t* bytes_written) {
    if (!file || !file->filesystem || !buffer || !bytes_written) {
        return 0;
    }

    if (file->filesystem->type == FILESYSTEM_FAT32) {
        return fat32_write_file(file, buffer, size, bytes_written);
    }

    return 0;
}

int filesystem_seek(FilesystemFile* file, int64_t offset, FilesystemSeekWhence whence) {
    if (!file || !file->filesystem || file->is_directory) {
        return 0;
    }

    int64_t base;

    switch (whence) {
        case FILESYSTEM_SEEK_SET:
            base = 0;
            break;

        case FILESYSTEM_SEEK_CURRENT:
            if (file->position > INT64_MAX) {
                return 0;
            }

            base = (int64_t)file->position;
            break;

        case FILESYSTEM_SEEK_END:
            if (file->size > INT64_MAX) {
                return 0;
            }

            base = (int64_t)file->size;
            break;

        default:
            return 0;
    }

    if (offset < 0 && base < -offset) {
        return 0;
    }

    int64_t new_position = base + offset;

    if (new_position < 0) {
        return 0;
    }

    file->position = (uint64_t)new_position;
    return 1;
}

int filesystem_close(FilesystemFile* file) {
    if (!file) {
        return 0;
    }

    file->filesystem = 0;
    file->first_cluster = 0;
    file->size = 0;
    file->position = 0;
    file->directory_lba = 0;
    file->directory_offset = 0;
    file->is_directory = 0;
    return 1;
}

int filesystem_open_path(Filesystem* filesystem, const char* path, FilesystemFile* file) {
    if (!filesystem || !path || !file) {
        return 0;
    }

    if (filesystem->type == FILESYSTEM_FAT32) {
        return fat32_open_path(filesystem, path, file);
    }

    return 0;
}

int filesystem_create_directory_path(Filesystem* filesystem, const char* path) {
    if (!filesystem || !path) {
        return 0;
    }

    if (filesystem->type == FILESYSTEM_FAT32) {
        return fat32_create_directory_path(filesystem, path);
    }

    return 0;
}

int filesystem_create_file_path(Filesystem* filesystem, const char* path) {
    if (!filesystem || !path) {
        return 0;
    }

    if (filesystem->type == FILESYSTEM_FAT32) {
        return fat32_create_file_path(filesystem, path);
    }

    return 0;
}

int filesystem_delete_directory(Filesystem* filesystem, const char* path) {
    if (!filesystem || !path) {
        return 0;
    }

    if (filesystem->type == FILESYSTEM_FAT32) {
        return fat32_delete_directory(filesystem, path);
    }

    return 0;
}

int filesystem_delete_file(Filesystem* filesystem, const char* path) {
    if (!filesystem || !path) {
        return 0;
    }

    if (filesystem->type == FILESYSTEM_FAT32) {
        return fat32_delete_file(filesystem, path);
    }

    return 0;
}