#include "vfs.h"

static Vfs g_vfs;

static uint32_t vfs_length(const char* s) {
    uint32_t length = 0;

    if (!s) {
        return 0;
    }

    while (s[length] != '\0') {
        length++;
    }

    return length;
}

static int vfs_compare(const char* a, const char* b) {
    if (!a && !b) {
        return 0;
    }

    if (!a) {
        return -1;
    }

    if (!b) {
        return 1;
    }

    uint32_t i = 0;

    while (a[i] != '\0' && b[i] != '\0') {
        if ((unsigned char)a[i] != (unsigned)b[i]) {
            return (unsigned char)a[i] - (unsigned char)b[i];
        }

        i++;
    }

    if (a[i] == '\0' && b[i] == '\0') {
        return 0;
    }

    if (a[i] == '\0') {
        return -1;
    }

    return 1;
}

static int vfs_ncompare(const char* a, const char* b, uint32_t n) {
    if (!a || !b) {
        return 0;
    }

    uint32_t i = 0;

    while (i < n) {
        if (a[i] == '\0' && b[i] == '\0') {
            return 0;
        }

        if ((unsigned char)a[i] != (unsigned char)b[i]) {
            return (unsigned char)a[i] - (unsigned char)b[i];
        }

        i++;
    }

    return 0;
}

static int vfs_copy(char* destination, uint32_t destination_size, const char* source) {
    if (!destination || !source || destination_size == 0) {
        return 0;
    }

    uint32_t i = 0;

    while (source[i] != '\0' && i + 1 < destination_size) {
        destination[i] = source[i];
        i++;
    }

    destination[i] = '\0';
    return 1;
}

static int vfs_start_with(const char* path, const char* prefix) {
    if (!path || !prefix) {
        return 0;
    }

    uint32_t i = 0;

    while (prefix[i] != '\0') {
        if (path[i] != prefix[i]) {
            return 0;
        }

        i++;
    }

    return 1;
}

static int vfs_match_mount(const char* path, const char* mount_point) {
    if (!path || !mount_point) {
        return 0;
    }

    if (mount_point[0] == '/' && mount_point[1] == '\0') {
        return 1;
    }

    uint32_t path_length = vfs_length(path);
    uint32_t mount_length = vfs_length(mount_point);

    if (mount_length > path_length) {
        return 0;
    }

    if (vfs_ncompare(path, mount_point, mount_length) != 0) {
        return 0;
    }

    if (path[mount_length] == '\0') {
        return 1;
    }

    if (path[mount_length] == '/') {
        return 1;
    }

    return 0;
}

static int vfs_make_relative_path(const char* path, const char* mount_point, char* out, uint32_t out_size) {
    if (!path || !mount_point || !out || out_size == 0) {
        return 0;
    }

    const char* relative = path;
    uint32_t mount_length;

    if (!(mount_point[0] == '/' && mount_point[1] == '\0')) {
        mount_length = vfs_length(mount_point);

        if (vfs_ncompare(path, mount_point, mount_length) != 0) {
            return 0;
        }

        relative = path + mount_length;
    }

    while (*relative == '/') {
        relative++;
    }

    if (*relative == '\0') {
        if (out_size < 2) {
            return 0;
        }

        out[0] = '/';
        out[1] = '\0';
        return 1;
    }

    uint32_t relative_length = vfs_length(relative);

    if (relative_length + 2 > out_size) {
        return 0;
    }

    out[0] = '/';

    for (uint32_t i = 0; i < relative_length; i++) {
        out[i + 1] = relative[i];
    }

    out[relative_length + 1] = '\0';
    return 1;
}

void vfs_init(void) {
    for (uint32_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        g_vfs.mounts[i].mount_point[0] = '\0';
        g_vfs.mounts[i].filesystem = 0;
    }

    g_vfs.mount_count = 0;
}

int vfs_mount_root(Filesystem* filesystem) {
    if (!filesystem) {
        return 0;
    }

    vfs_init();
    g_vfs.mounts[0].filesystem = filesystem;
    g_vfs.mounts[0].mount_point[0] = '/';
    g_vfs.mounts[0].mount_point[1] = '\0';
    g_vfs.mount_count = 1;
    return 1;
}

int vfs_mount(const char* mount_point, Filesystem* filesystem) {
    if (!mount_point || !filesystem) {
        return 0;
    }

    if (g_vfs.mount_count >= VFS_MAX_MOUNTS) {
        return 0;
    }

    for (uint32_t i = 0; i < g_vfs.mount_count; i++) {
        if (vfs_compare(g_vfs.mounts[i].mount_point, mount_point) == 0) {
            return 0;
        }
    }

    g_vfs.mounts[g_vfs.mount_count].filesystem = filesystem;
    vfs_copy(g_vfs.mounts[g_vfs.mount_count].mount_point, VFS_PATH_MAX, mount_point);
    g_vfs.mount_count++;
    return 1;
}

int vfs_resolve_path(const char* path, Filesystem** filesystem, char* relative_path, uint32_t relative_path_size) {
    if (!path || !filesystem || !relative_path || relative_path_size == 0) {
        return 0;
    }

    *filesystem = 0;

    if (g_vfs.mount_count == 0) {
        return 0;
    }

    uint32_t best_index = 0;
    uint32_t best_length = 0;

    for (uint32_t i = 0; i < g_vfs.mount_count; i++) {
        if (!g_vfs.mounts[i].filesystem) {
            continue;
        }

        if (!vfs_match_mount(path, g_vfs.mounts[i].mount_point)) {
            continue;
        }

        uint32_t length = vfs_length(g_vfs.mounts[i].mount_point);

        if (length > best_length) {
            best_length = length;
            best_index = i;
        }
    }

    *filesystem = g_vfs.mounts[best_index].filesystem;

    if (!vfs_make_relative_path(path, g_vfs.mounts[best_index].mount_point, relative_path, relative_path_size)) {
        return 0;
    }

    return 1;
}

int vfs_open_path(const char* path, FilesystemFile* file) {
    if (!path || !file) {
        return 0;
    }

    Filesystem* filesystem = 0;
    char relative[VFS_PATH_MAX];

    if (!vfs_resolve_path(path, &filesystem, relative, sizeof(relative))) {
        return 0;
    }

    return filesystem_open_path(filesystem, relative, file);
}

int vfs_create_directory_path(const char* path) {
    if (!path) {
        return 0;
    }

    Filesystem* filesystem = 0;
    char relative[VFS_PATH_MAX];

    if (!vfs_resolve_path(path, &filesystem, relative, sizeof(relative))) {
        return 0;
    }

    if (vfs_compare(relative, "/") == 0) {
        return 0;
    }

    return filesystem_create_directory_path(filesystem, relative);
}

int vfs_create_file_path(const char* path) {
    if (!path) {
        return 0;
    }

    Filesystem* filesystem = 0;
    char relative[VFS_PATH_MAX];

    if (!vfs_resolve_path(path, &filesystem, relative, sizeof(relative))) {
        return 0;
    }

    if (vfs_compare(relative, "/") == 0) {
        return 0;
    }

    return filesystem_create_file_path(filesystem, relative);
}

int vfs_delete_directory(const char* path) {
    if (!path) {
        return 0;
    }

    Filesystem* filesystem = 0;
    char relative[VFS_PATH_MAX];

    if (!vfs_resolve_path(path, &filesystem, relative, sizeof(relative))) {
        return 0;
    }

    if (vfs_compare(relative, "/") == 0) {
        return 0;
    }

    return filesystem_delete_directory(filesystem, relative);
}

int vfs_delete_file(const char* path) {
        if (!path) {
        return 0;
    }

    Filesystem* filesystem = 0;
    char relative[VFS_PATH_MAX];

    if (!vfs_resolve_path(path, &filesystem, relative, sizeof(relative))) {
        return 0;
    }

    if (vfs_compare(relative, "/") == 0) {
        return 0;
    }

    return filesystem_delete_file(filesystem, relative);
}

int vfs_read(FilesystemFile* file, void* buffer, uint32_t size, uint32_t* bytes_read) {
    return filesystem_read(file, buffer, size, bytes_read);
}

int vfs_write(FilesystemFile* file, const void* buffer, uint32_t size, uint32_t* bytes_written) {
    return filesystem_write(file, buffer, size, bytes_written);
}

int vfs_seek(FilesystemFile* file, int64_t offset, FilesystemSeekWhence whence) {
    return filesystem_seek(file, offset, whence);
}

int vfs_close(FilesystemFile* file) {
    return filesystem_close(file);
}