#include "storage.h"
#include "block.h"
#include "vfs.h"
#include "partition/partition.h"
#include "../interrupts/interrupts.h"
#include "../core/log.h"

#define MAX_STORAGE_FILESYSTEMS 16

static Filesystem storage_filesystems[MAX_STORAGE_FILESYSTEMS];
static uint32_t storage_filesystem_count = 0;

static void storage_test_path() {
    FilesystemFile file;

    if (!vfs_open_path("/system/test", &file)) {
        kernel_log("Test path does not exist! creating it...");

        if (!vfs_create_directory_path("/system/test")) {
            kernel_panic("path creating failed");
        }

        if (!vfs_open_path("/system/test", &file)) {
            kernel_panic("created path cannot be opened");
        }
    }

    if (!file.is_directory) {
        vfs_close(&file);
        kernel_panic("file is not a directory");
    }

    if (!vfs_delete_directory("/system/test")) {
        kernel_panic("deleting directory failed");
    }

    vfs_close(&file);
    kernel_log("Nested path test passed.");
}

static void storage_test_file() {
    FilesystemFile file;

    if (!vfs_open_path("/test.txt", &file)) {
        kernel_log("Test.txt does not exist! creating it...");

        if (!vfs_create_file_path("/test.txt")) {
            kernel_panic("file creation failed");
        }

        if (!vfs_open_path("/test.txt", &file)) {
            kernel_panic("created file cannot be opened");
        }
    }

    static const char test_text[] = "Belqen filesystem write test.\n";
    uint32_t bytes_written = 0;

    if (!vfs_seek(&file, 0, FILESYSTEM_SEEK_SET)) {
        vfs_close(&file);
        kernel_panic("seek failed");
    }

    if (!vfs_write(&file, test_text, sizeof(test_text) - 1, &bytes_written)) {
        vfs_close(&file);
        kernel_panic("write failed");
    }

    if (bytes_written != sizeof(test_text) - 1) {
        vfs_close(&file);
        kernel_panic("incomplete write");
    }

    if (!vfs_seek(&file, 0, FILESYSTEM_SEEK_SET)) {
        vfs_close(&file);
        kernel_panic("rewind failed");
    }

    char read_back[sizeof(test_text)];
    uint32_t bytes_read = 0;

    if (!vfs_read(&file, read_back, sizeof(test_text) - 1, &bytes_read)) {
        vfs_close(&file);
        kernel_panic("readback failed");
    }

    read_back[bytes_read] = '\0';

    for (uint32_t i = 0; i < bytes_read; i++) {
        if (read_back[i] != test_text[i]) {
            vfs_close(&file);
            kernel_panic("readback mismatch");
        }
    }

    if (!vfs_delete_file("/test.txt")) {
        kernel_panic("deleting file failed");
    }

    vfs_close(&file);
    kernel_log("Storage file test passed.");
}

static void storage_test(void) {
    if (storage_filesystem_count == 0) {
        kernel_log("No filesystem for file test.");
        return;
    }

    storage_test_path();
    storage_test_file();
}

static void storage_create_directory_tree(void) {
    if (storage_filesystem_count == 0) {
        kernel_log("No filesystem for file test.");
        return;
    }

    const char* directories[] = {
        "/system",
        "/system/apps",
        "/system/apps/terminal",
        "/system/apps/filebrowser",
        "/system/apps/diskmanager",
        "/system/apps/processmanager",
        "/system/apps/logs",
        "/system/logs"
        "/users",
        "/users/belqen",
        "/users/belqen/desktop",
        "/users/belqen/documents",
        "/users/belqen/downloads",
        "/users/belqen/pictures",
        "/users/belqen/music",
        "/users/belqen/videos",
        "/apps",
        "/games",
        "/cache",
        "/temp"
    };

    uint32_t count = sizeof(directories) / sizeof(directories[0]);

    for (uint32_t i = 0; i < count; i++) {
        if (!vfs_create_directory_path(directories[i])) {
            kernel_log("Could not create directory %s", directories[i]);
            kernel_panic("failed to create directory tree");
        }
    }

    kernel_log("Belqen directory tree created.");
}

void storage_init(void) {
    kernel_log("initializing storage...");
    uint32_t device_count = block_get_device_count();
    kernel_log("block devices %u", device_count);

    for (uint32_t i = 0; i < device_count; i++) {
        BlockDevice* device = block_get_device(i);

        if (!device) {
            continue;
        }

        if (device->type != BLOCK_DEVICE_PHYSICAL) { 
            continue;
        }

        partition_scan(device);
    }

    for (uint32_t i = 0; i < device_count; i++) {
        BlockDevice* disk = block_get_device(i);

        if (!disk) {
            continue;
        }

        if (disk->type != BLOCK_DEVICE_PHYSICAL) { 
            continue;
        }

        for (uint32_t partition_number = 0; partition_number < 4; partition_number++) {
            if (storage_filesystem_count >= MAX_STORAGE_FILESYSTEMS) {
                break;
            }

            BlockDevice* partition = partition_get_device(disk, partition_number);

            if (!partition) {
                continue;
            }

            Filesystem* filesystem = &storage_filesystems[storage_filesystem_count];
            kernel_log("trying filesystem on disk %u partition %u", i, partition_number);

            if (filesystem_mount(partition, filesystem))  {
                kernel_log("filesystem mounted on disk %u partition %u", i, partition_number);
                storage_filesystem_count++;
            }
            else {
                kernel_log("no supported filesystem on disk %u partition %u", i, partition_number);
            }
        }
    }

    kernel_log("mounted filesystems %u", storage_filesystem_count);

    if (storage_filesystem_count > 0) {
        if (!vfs_mount_root(&storage_filesystems[0])) {
            kernel_panic("mounting root filesystem failed");
        }

        kernel_log("Root filesystem mounted.");
    }

    kernel_log("block devices after partition scan %u", block_get_device_count());
    kernel_log("storage initialized.");
    storage_test();
    storage_create_directory_tree();
}

uint32_t storage_get_filesystem_count(void) {
    return storage_filesystem_count;
}

Filesystem* storage_get_filesystem(uint32_t index) {
    if (index >= storage_filesystem_count) {
        return 0;
    }

    return &storage_filesystems[index];
}

int storage_mount_partition(BlockDevice* partition) {
    if (!partition) {
        return 0;
    }

    if (partition->type != BLOCK_DEVICE_PARTITION) {
        return 0;
    }

    for (uint32_t i = 0; i < storage_filesystem_count; i++) {
        if (storage_filesystems[i].device == partition) {
            return 1;
        }
    }

    if (storage_filesystem_count >= MAX_STORAGE_FILESYSTEMS) {
        kernel_log("storage filesystem list is full.");
        return 0;
    }

    Filesystem* filesystem = &storage_filesystems[storage_filesystem_count];
    kernel_log("trying to mount runtime partition as filesystem.");

    if (!filesystem_mount(partition, filesystem)) {
        kernel_log("runtime filesystem mount failed.");
        return 0;
    }

    storage_filesystem_count++;
    kernel_log("runtime filesystem mounted. total %u", storage_filesystem_count);
    return 1;
}

int storage_unmount_partition(BlockDevice* partition) {
    if (!partition) {
        return 0;
    }

    for (uint32_t i = 0; i < storage_filesystem_count; i++) {
        if (storage_filesystems[i].device != partition) {
            continue;
        }

        for (uint32_t j = i; j + 1 < storage_filesystem_count; j++) {
            storage_filesystems[j] = storage_filesystems[j + 1];
        }

        storage_filesystem_count--;
        storage_filesystems[storage_filesystem_count].device = 0;
        storage_filesystems[storage_filesystem_count].type = FILESYSTEM_UNKNOWN;
        storage_filesystems[storage_filesystem_count].filesystem_data = 0;
        kernel_log("filesystem unmounted. total %u", storage_filesystem_count);
        return 1;
    }

    return 0;
}

int storage_mount_root(Filesystem* filesystem) {
    return vfs_mount_root(filesystem);
}