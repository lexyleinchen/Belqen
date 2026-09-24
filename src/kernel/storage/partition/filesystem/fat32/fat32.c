#include "fat32.h"
#include "../../../../core/log.h"
#include "../../../../core/task.h"

#define FAT32_LFN_ATTRIBUTE 0x0F
#define FAT32_LFN_MAX_CHARS 255

typedef struct {
    uint32_t lba;
    uint32_t offset;
} Fat32DirectorySlot;

static uint16_t read_u16_le(const uint8_t* buffer) {
    return (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
}

static void write_u16_le(uint8_t* buffer, uint16_t value) {
    buffer[0] = (uint8_t)(value & 0xFF);
    buffer[1] = (uint8_t)((value >> 8) & 0xFF);
}

static uint32_t read_u32_le(const uint8_t* buffer) {
    return (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) | ((uint32_t)buffer[2] << 16) |  ((uint32_t)buffer[3] << 24);
}

static void write_u32_le(uint8_t* buffer, uint32_t value) {
    buffer[0] = (uint8_t)(value & 0xFF);
    buffer[1] = (uint8_t)((value >> 8) & 0xFF);
    buffer[2] = (uint8_t)((value >> 16) & 0xFF);
    buffer[3] = (uint8_t)((value >> 24) & 0xFF);
}

static uint32_t fat32_calculate_fat_size(uint64_t total_sectors, uint32_t reserved_sectors, uint32_t sectors_per_cluster, uint32_t fat_count) {
    uint32_t fat_sectors = 1;

    for (int i = 0; i < 10; i++) {
        uint64_t data_sectors = total_sectors - reserved_sectors - (uint64_t)fat_count * fat_sectors;
        uint64_t cluster_count = data_sectors / sectors_per_cluster;
        uint64_t fat_bytes = (cluster_count + 2) * 4;
        uint32_t new_fat_sectors = (uint32_t)((fat_bytes + 511) / 512);

        if (new_fat_sectors == fat_sectors) {
            return fat_sectors;
        }

        fat_sectors = new_fat_sectors;
    }

    return fat_sectors;
}

static FAT32FormatStatus fat32_format_status(FAT32FormatWork* work) {
    if (!work) {
        return FAT32_FORMAT_FAILED;
    }

    if (work->stage == 0) {
        return FAT32_FORMAT_IDLE;
    }

    if (work->stage == 100) {
        return FAT32_FORMAT_COMPLETE;
    }

    if (work->stage == 0xFFFFFFFF) {
        return FAT32_FORMAT_FAILED;
    }

    return FAT32_FORMAT_RUNNING;
}

static int fat32_format_step(FAT32FormatWork* work) {
    if (!work || !work->device) {
        return 1;
    }

    if (work->stage == 1) {
        if (work->fat_index < work->reserved_sector_count - 2) {
            uint32_t lba = 2 + work->fat_index;

            for (uint32_t i = 0; i < 512; i++) {
                work->sector[i] = 0;
            }

            if (!block_write(work->device, lba, 1, work->sector)) {
                kernel_log("fat32 failed to clear reserved sector.");
                work->stage = 0xFFFFFFFF;
                return 1;
            }

            work->fat_index++;
            work->progress++;
            return 0;
        }

        work->fat_index = 0;
        work->stage = 2;
        return 0;
    }

    if (work->stage == 2) {
        for (uint32_t i = 0; i < 512; i++) {
            work->sector[i] = 0;
        }

        work->sector[0] = 0xEB;
        work->sector[1] = 0x58;
        work->sector[2] = 0x90;
        work->sector[3] = 'B';
        work->sector[4] = 'e';
        work->sector[5] = 'l';
        work->sector[6] = 'q';
        work->sector[7] = 'e';
        work->sector[8] = 'n';
        work->sector[9] = ' ';
        work->sector[10] = ' ';
        write_u16_le(&work->sector[11], 512);
        work->sector[13] = (uint8_t)work->sectors_per_cluster;
        write_u16_le(&work->sector[14], (uint16_t)work->reserved_sector_count);
        work->sector[16] = (uint8_t)work->fat_count;
        write_u16_le(&work->sector[17], 0);
        write_u16_le(&work->sector[19], 0);
        work->sector[21] = 0xF8;
        write_u16_le(&work->sector[22], 0);
        write_u16_le(&work->sector[24], 63);
        write_u16_le(&work->sector[26], 255);
        write_u32_le(&work->sector[28], 0);
        write_u32_le(&work->sector[32], work->total_sectors);
        write_u32_le(&work->sector[36], work->fat_sectors);
        write_u16_le(&work->sector[40], 0);
        write_u16_le(&work->sector[42], 0);
        write_u32_le(&work->sector[44], 2);
        write_u16_le(&work->sector[48], 1);
        write_u16_le(&work->sector[50], 6);
        work->sector[64] = 0x80;
        work->sector[66] = 0x29;
        write_u32_le(&work->sector[67], 0x50524E54);
        work->sector[71] = 'B';
        work->sector[72] = 'e';
        work->sector[73] = 'l';
        work->sector[74] = 'q';
        work->sector[75] = 'e';
        work->sector[76] = 'n';
        work->sector[77] = '-';
        work->sector[82] = 'F';
        work->sector[83] = 'A';
        work->sector[84] = 'T';
        work->sector[85] = '3';
        work->sector[86] = '2';
        work->sector[510] = 0x55;
        work->sector[511] = 0xAA;

        if (!block_write(work->device, 0, 1, work->sector)) {
            kernel_log("fat32 failed to write boot sector");
            work->stage = 0xFFFFFFFF;
            return 1;
        }

        work->progress++;
        work->stage = 3;
        return 0;
    }

    if (work->stage == 3) {
        if (!block_write(work->device, 6, 1, work->sector)) {
            kernel_log("fat32 failed to write backup boot sector.");
            work->stage = 0xFFFFFFFF;
            return 1;
        }

        work->progress++;
        work->stage = 4;
        return 0;
    }

    if (work->stage == 4) {
        for (uint32_t i = 0; i < 512; i++) {
            work->sector[i] = 0;
        }

        write_u32_le(&work->sector[0], 0x41615252);
        write_u32_le(&work->sector[484], 0x61417272);
        write_u32_le(&work->sector[488], 0xFFFFFFFF);
        write_u32_le(&work->sector[492], 3);
        write_u32_le(&work->sector[508], 0xAA550000);

        if (!block_write(work->device, 1, 1, work->sector)) {
            kernel_log("fat32 failed to write fat32 fsinfo.");
            work->stage = 0xFFFFFFFF;
            return 1;
        }

        work->progress++;
        work->fat_index = 0;
        work->stage = 5;
        return 0;
    }

    if (work->stage == 5) {
        if (work->fat_index < work->fat_count * work->fat_sectors) {
            uint32_t fat_sector_index = work->fat_index;
            uint32_t fat = fat_sector_index / work->fat_sectors;
            uint32_t sector_index = fat_sector_index % work->fat_sectors;
            uint32_t lba = work->reserved_sector_count + fat * work->fat_sectors + sector_index;

            for (uint32_t i = 0; i < 512; i++) {
                work->sector[i] = 0;
            }

            if (sector_index == 0) {
                write_u32_le(&work->sector[0], 0x0FFFFFF8);
                write_u32_le(&work->sector[4], 0xFFFFFFFF);
                write_u32_le(&work->sector[8], 0x0FFFFFFF);
            }

            if (!block_write(work->device, lba, 1, work->sector)) {
                kernel_log("fat32 failed to write fat.");
                work->stage = 0xFFFFFFFF;
                return 1;
            }

            work->fat_index++;
            work->progress++;
            return 0;
        }

        work->fat_index = 0;
        work->stage = 6;
        return 0;
    }

    if (work->stage == 6) {
        if (work->fat_index < work->sectors_per_cluster) {
            for (uint32_t i = 0; i < 512; i++) {
                work->sector[i] = 0;
            }

            if (!block_write(work->device, work->root_lba + work->fat_index, 1, work->sector)) {
                kernel_log("fat32 failed to clear root directory.");
                work->stage = 0xFFFFFFFF;
                return 1;
            }

            work->fat_index++;
            work->progress++;
            return 0;
        }

        work->stage = 100;
        kernel_log("fat32 format complete.");
        return 1;
    }

    return 1;
}

static void fat32_format_thread(void* arg) {
    FAT32FormatWork* work = (FAT32FormatWork*)arg;

    while (!fat32_format_step(work)) {
        thread_yield();
    }
}

int fat32_format_async(BlockDevice* device, FAT32FormatWork* work) {
    if (!device || !work) {
        return 0;
    }

    if (device->sector_size != 512) {
        kernel_log("fat32 device must use 512 byte sectors.");
        return 0;
    }

    if (device->sector_count < 65536) {
        kernel_log("fat32 disk is too small for fat32.");
        return 0;
    }

    const uint32_t sectors_per_cluster = 8;
    const uint32_t reserved_sectors = 32;
    const uint32_t fat_count = 2;
    uint64_t total_sectors = device->sector_count;
    uint32_t fat_sectors = fat32_calculate_fat_size(total_sectors, reserved_sectors, sectors_per_cluster, fat_count);
    uint64_t data_sectors = total_sectors - reserved_sectors - (uint64_t)fat_count * fat_sectors;
    uint32_t cluster_count = (uint32_t)(data_sectors / sectors_per_cluster);

    if (cluster_count < 65525) {
        kernel_log("fat32 calculated filesystem is not fat32.");
        return 0;
    }

    for (uint32_t i = 0; i < sizeof(FAT32FormatWork); i++) {
        ((uint8_t*)work)[i] = 0;
    }

    work->device = device;
    work->stage = 1;
    work->progress = 0;
    work->fat_sectors = fat_sectors;
    work->fat_index = 0;
    work->total_sectors = (uint32_t)total_sectors;
    work->sectors_per_cluster = sectors_per_cluster;
    work->reserved_sector_count = reserved_sectors;
    work->fat_count = fat_count;
    work->root_lba = reserved_sectors + fat_count * fat_sectors;
    uint32_t reserved_work = reserved_sectors - 2;
    uint32_t fat_work = fat_count * fat_sectors;
    work->total_progress = reserved_work + 1 + 1 + 1 + fat_work + sectors_per_cluster;
    Task* task = task_create("fat32_format", (void*)fat32_format_thread, work, 1);

    if (!task) {
        kernel_log("fat32 failed to start format thread.");
        work->stage = 0xFFFFFFFF;
        return 0;
    }

    work->work_id = (int)task->tid;
    kernel_log("fat32 format thread started.");
    return 1;
}

FAT32FormatStatus fat32_format_get_status(FAT32FormatWork* work) {
    return fat32_format_status(work);
}

uint32_t fat32_format_get_progress(FAT32FormatWork* work) {
    if (!work) {
        return 0;
    }

    return work->progress;
}

uint32_t fat32_format_get_percent(FAT32FormatWork* work) {
    if (!work || work->total_progress == 0) {
        return 0;
    }
    
    if (work->progress >= work->total_progress) {
        return 100;
    }

    return (work->progress * 100) / work->total_progress;
}

int fat32_mount(BlockDevice* device, Filesystem* filesystem) {
    if (!device || !filesystem) {
        return 0;
    }

    uint8_t sector[512];

    if (!block_read(device, 0, 1, sector)) {
        kernel_log("fat32 failed to read boot sector.");
        return 0;
    }

    if (sector[510] != 0x55 || sector[511] != 0xAA) {
        kernel_log("fat32 invalid boot sector signature.");
        return 0;
    }

    FAT32Filesystem* fat32 = (FAT32Filesystem*)filesystem->filesystem_data;

    if (!fat32) {
        return 0;
    }

    fat32->bytes_per_sector = read_u16_le(&sector[11]);
    fat32->sectors_per_cluster = sector[13];
    fat32->reserved_sector_count = read_u16_le(&sector[14]);
    fat32->fat_count = sector[16];
    fat32->sectors_per_fat = read_u32_le(&sector[36]);
    fat32->root_cluster = read_u32_le(&sector[44]);

    if (fat32->bytes_per_sector != device->sector_size) {
        kernel_log("fat32 sector size dose not match block device.");
        return 0;
    }

    if (fat32->bytes_per_sector != 512) {
        kernel_log("fat32 currently requires 512 byte sectors.");
        return 0;
    }

    if (fat32->sectors_per_cluster == 0 || fat32->sectors_per_fat == 0 || fat32->fat_count == 0) {
        kernel_log("invalid fat32 filesystem parameters.");
        return 0;
    }

    fat32->fat_start_lba = fat32->reserved_sector_count;
    fat32->data_start_lba = fat32->reserved_sector_count + fat32->fat_count * fat32->sectors_per_fat;

    if (fat32->data_start_lba >= device->sector_count) {
        kernel_log("fat32 data region is outside partition.");
        return 0;
    }

    uint32_t data_sectors = (uint32_t)device->sector_count - fat32->data_start_lba;
    fat32->cluster_count = data_sectors / fat32->sectors_per_cluster;

    if (fat32->cluster_count < 65525) {
        kernel_log("not actually fat32.");
        return 0;
    }

    if (fat32->root_cluster < 2) {
        kernel_log("invalid fat32 root cluster.");
        return 0;
    }

    if (fat32->root_cluster >= fat32->cluster_count + 2) {
        kernel_log("fat32 root cluster is outside filesystem.");
        return 0;
    }

    filesystem->device = device;
    filesystem->type = FILESYSTEM_FAT32;
    kernel_log("fat32 filesystem mounted.");
    kernel_log("bytes per sector %u.", fat32->bytes_per_sector);
    kernel_log("sector per cluster %u.", fat32->sectors_per_cluster);
    kernel_log("reversed sectors %u.", fat32->reserved_sector_count);
    kernel_log("fat count %u.", fat32->fat_count);
    kernel_log("sectors per fat %u.", fat32->sectors_per_fat);
    kernel_log("root cluster %u.", fat32->root_cluster);
    kernel_log("data start lba %u.", fat32->data_start_lba);
    return 1;
}

static uint32_t fat32_cluster_to_lba(FAT32Filesystem* fat32, uint32_t cluster) {
    if (!fat32) {
        return 0;
    }

    if (cluster < 2) {
        return 0;
    }

    if (cluster >= fat32->cluster_count + 2) {
        return 0;
    }

    return fat32->data_start_lba + (cluster - 2) * fat32->sectors_per_cluster;
}

static uint32_t fat32_get_next_cluster(Filesystem* filesystem, uint32_t cluster) {
    if (!filesystem) {
        return 0;
    }

    FAT32Filesystem* fat32 = (FAT32Filesystem*)filesystem->filesystem_data;

    if (!fat32) {
        return 0;
    }

    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat32->fat_start_lba + (fat_offset / fat32->bytes_per_sector);
    uint32_t offset = fat_offset % fat32->bytes_per_sector;
    uint8_t sector[512];

    if (!block_read(filesystem->device, fat_sector, 1, sector)) {
        return 0;
    }

    uint32_t value = read_u32_le(&sector[offset]);
    return value & 0x0FFFFFFF;
}

static void fat32_copy_name(const uint8_t* entry, char* name) {
    int position = 0;

    for (int i = 0; i < 8; i++) {
        if (entry[i] == ' ') {
            break;
        }

        name[position++] = entry[i];
    }

    if (entry[8] != ' ') {
        name[position++] = '.';

        for (int i = 8; i < 11; i++) {
            if (entry[i] == ' ') {
                break;
            }

            name[position++] = entry[i];
        }
    }

    name[position] = '\0';
}

static int fat32_name_equal(const char* left, const char* right) {
    uint32_t index = 0;

    while (left[index] != '\0' && right[index] != '\0') {
        char left_char = left[index];
        char right_char = right[index];

        if (left_char >= 'a' && left_char <= 'z') {
            left_char -= 32;
        }

        if (right_char >= 'a' && right_char <= 'z') {
            right_char -= 32;
        }

        if (left_char != right_char) {
            return 0;
        }

        index++;
    }

    return left[index] == '\0' && right[index] == '\0';
}

static uint8_t fat32_short_name_checksum(const uint8_t name[11]) {
    uint8_t checksum = 0;

    for (uint32_t i = 0; i < 11; i++) {
        checksum = ((checksum & 1) ? 0x80 : 0) + (checksum >> 1) + name[i];
    }

    return checksum;
}

static void fat32_lfn_clear(char* name) {
    for (uint32_t i = 0; i <= FAT32_LFN_MAX_CHARS; i++) {
        name[i] = '\0';
    }
}

static void fat32_lfn_copy_entry(const uint8_t* entry, char* name) {
    uint32_t sequence = entry[0] & 0x1F;

    if (sequence == 0 || sequence > 20) {
        return;
    }

    uint32_t start = (sequence - 1) * 13;
    const uint32_t offsets[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};

    for (uint32_t i = 0; i <13; i++) {
        uint16_t character = read_u16_le(&entry[offsets[i]]);

        if (character == 0x0000) {
            return;
        }

        if (character == 0xFFFF) {
            continue;
        }

        uint32_t position = start + i;

        if (position >= FAT32_LFN_MAX_CHARS) {
            continue;
        }

        if (character > 0x7F) {
            name[position] = '?';
        }
        else {
            name[position] = (char)character;
        }
    }

    name[FAT32_LFN_MAX_CHARS] = '\0';
}

static int fat32_find_entry(Filesystem* filesystem, uint32_t directory_cluster, const char* requested_name, FilesystemFile* file) {
    if (!filesystem || !requested_name || !file) {
        return 0;
    }

    FAT32Filesystem* fat32 = (FAT32Filesystem*)filesystem->filesystem_data;

    if (!fat32) {
        return 0;
    }

    if (directory_cluster < 2) {
        directory_cluster = fat32->root_cluster;
    }

    uint32_t cluster = directory_cluster;
    uint8_t sector[512];
    char long_name[FAT32_LFN_MAX_CHARS + 1];
    uint8_t long_name_checksum = 0;
    int long_name_active = 0;
    fat32_lfn_clear(long_name);

    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        uint32_t lba = fat32_cluster_to_lba(fat32, cluster);

        if (!lba) {
            return 0;
        }

        for (uint32_t sector_index = 0; sector_index < fat32->sectors_per_cluster; sector_index++) {
            if (!block_read(filesystem->device, lba + sector_index, 1, sector)) {
                return 0;
            }

            for (uint32_t offset = 0; offset < 512; offset += 32) {
                uint8_t* entry = &sector[offset];

                if (entry[0] == 0x00) {
                    return 0;
                }

                if (entry[0] == 0xE5) {
                    long_name_active = 0;
                    fat32_lfn_clear(long_name);
                    continue;
                }

                if (entry[11] == FAT32_LFN_ATTRIBUTE) {
                    uint8_t sequence = entry[0] & 0x1F;
                    
                    if (sequence == 0 || sequence > 20) {
                        long_name_active = 0;
                        continue;
                    }

                    if (entry[0] & 0x40) {
                        fat32_lfn_clear(long_name);
                        long_name_checksum = entry[13];
                        long_name_active = 1;
                    }

                    if (long_name_active && entry[13] == long_name_checksum) {
                        fat32_lfn_copy_entry(entry, long_name);
                    }

                    continue;
                }

                if (entry[11] & 0x08) {
                    long_name_active = 0;
                    fat32_lfn_clear(long_name);
                    continue;
                }

                char short_name[256];
                fat32_copy_name(entry, short_name);
                const char* entry_name = short_name;
                uint8_t short_name_bytes[11];

                for (uint32_t i = 0; i < 11; i++) {
                    short_name_bytes[i] = entry[i];
                }

                if (long_name_active && fat32_short_name_checksum(short_name_bytes) == long_name_checksum && long_name[0] != '\0') {
                    entry_name = long_name;
                }

                if (!fat32_name_equal(entry_name, requested_name)) {
                    long_name_active = 0;
                    fat32_lfn_clear(long_name);
                    continue;
                }

                uint32_t high = (uint32_t)read_u16_le(&entry[20]);
                uint32_t low = (uint32_t)read_u16_le(&entry[26]);
                file->filesystem = filesystem;
                file->first_cluster = (high << 16) | low;
                file->size = read_u32_le(&entry[28]);
                file->position = 0;
                file->directory_lba = lba + sector_index;
                file->directory_offset = offset;
                file->is_directory = (entry[11] & 0x10) != 0;
                return 1;
            }
        }

        cluster = fat32_get_next_cluster(filesystem, cluster);
        long_name_active = 0;
        fat32_lfn_clear(long_name);
    }

    return 0;
}

int fat32_format(BlockDevice* device) {
    if (!device) {
        return 0;
    }

    if (device->sector_count < 65536) {
        kernel_log("disk is too small for fat32.");
        return 0;
    }

    const uint32_t bytes_per_sector = 512;
    const uint32_t sectors_per_cluster = 8;
    const uint32_t reserved_sectors = 32;
    const uint32_t fat_count = 2;
    uint64_t total_sectors = device->sector_count;
    uint32_t fat_sectors = fat32_calculate_fat_size(total_sectors, reserved_sectors, sectors_per_cluster, fat_count);
    uint64_t data_sectors = total_sectors - reserved_sectors - (uint64_t)fat_count * fat_sectors;
    uint32_t cluster_count = (uint32_t)(data_sectors / sectors_per_cluster);

    if (cluster_count < 65525) {
        kernel_log("calculated filesystem is not fat32.");
        return 0;
    }

    kernel_log("formatting fat32...");
    kernel_log("total sectors %u", (uint32_t)total_sectors);
    kernel_log("sectors per cluster %u", sectors_per_cluster);
    kernel_log("reserved sectors %u", reserved_sectors);
    kernel_log("fat sectors %u", fat_sectors);
    kernel_log("cluster count %u", cluster_count);
    uint8_t sector[512];

    for (uint32_t i = 0; i < 512; i++) {
        sector[i] = 0;
    }

    sector[0] = 0xEB;
    sector[1] = 0x58;
    sector[2] = 0x90;
    sector[3] = 'B';
    sector[4] = 'e';
    sector[5] = 'l';
    sector[6] = 'q';
    sector[7] = 'e';
    sector[8] = 'n';
    sector[9] = ' ';
    sector[10] = ' ';
    write_u16_le(&sector[11], bytes_per_sector);
    sector[13] = sectors_per_cluster;
    write_u16_le(&sector[14], reserved_sectors);
    sector[16] = fat_count;
    write_u16_le(&sector[17], 0);
    write_u16_le(&sector[19], 0);
    sector[21] = 0xF8;
    write_u16_le(&sector[22], 0);
    write_u16_le(&sector[24], 63);
    write_u16_le(&sector[26], 255);
    write_u32_le(&sector[28], 0);
    write_u32_le(&sector[32], (uint32_t)total_sectors);
    write_u32_le(&sector[36], fat_sectors);
    write_u16_le(&sector[40], 0);
    write_u16_le(&sector[42], 0);
    write_u32_le(&sector[44], 2);
    write_u16_le(&sector[48], 1);
    write_u16_le(&sector[50], 6);
    sector[64] = 0x80;
    sector[66] = 0x29;
    write_u32_le(&sector[67], 0x50524E54);
    sector[71] = 'B';
    sector[72] = 'e';
    sector[73] = 'l';
    sector[74] = 'q';
    sector[75] = 'e';
    sector[76] = 'n';
    sector[77] = ' ';
    sector[78] = ' ';
    sector[79] = ' ';
    sector[80] = ' ';
    sector[81] = ' ';
    sector[82] = 'F';
    sector[83] = 'A';
    sector[84] = 'T';
    sector[85] = '3';
    sector[86] = '2';
    sector[87] = ' ';
    sector[88] = ' ';
    sector[89] = ' ';
    sector[510] = 0x55;
    sector[511] = 0xAA;

    if (!block_write(device, 0, 1, sector)) {
        kernel_log("failed to write fat32 boot sector.");
        return 0;
    }

    if (!block_write(device, 6, 1, sector)) {
        kernel_log("failed to write fat32 backup boot sector.");
        return 0;
    }

    for (uint32_t i = 0; i < 512; i++) {
        sector[i] = 0;
    }

    write_u32_le(&sector[0], 0x41615252);
    write_u32_le(&sector[484], 0x61417272);
    write_u32_le(&sector[488], 0xFFFFFFFF);
    write_u32_le(&sector[492], 3);
    write_u32_le(&sector[508], 0xAA550000);

    if (!block_write(device, 1, 1, sector)) {
        kernel_log("failed to write fat32 fsinfo.");
        return 0;
    }

    for (uint32_t lba = 2; lba < reserved_sectors; lba++) {
        for (uint32_t i = 0; i < 512; i++) {
            sector[i] = 0;
        }

        if (!block_write(device, lba, 1, sector)) {
            kernel_log("failed to clear reserved sector.");
            return 0;
        }
    }

    uint32_t fat_start = reserved_sectors;

    for (uint32_t fat = 0; fat < fat_count; fat++) {
        uint32_t start = fat_start + fat * fat_sectors;

        for (uint32_t s = 0; s < fat_sectors; s++) {
            for (uint32_t i = 0; i < 512; i++) {
                sector[i] = 0;
            }

            if (s == 0) {
                write_u32_le(&sector[0], 0x0FFFFFF8);
                write_u32_le(&sector[4], 0xFFFFFFFF);
                write_u32_le(&sector[8], 0x0FFFFFFF);
            }

            if (!block_write(device, start + s, 1, sector)) {
                kernel_log("failed to write fat.");
                return 0;
            }
        }
    }

    uint32_t root_lba = reserved_sectors + fat_count * fat_sectors;

    for (uint32_t s = 0; s < sectors_per_cluster; s++) {
        for (uint32_t i = 0; i < 512; i++) {
            sector[i] = 0;
        }

        if (!block_write(device, root_lba + s, 1, sector)) {
            kernel_log("failed to clear root directory.");
            return 0;
        }
    }

    kernel_log("fat32 format complete.");
    return 1;
}

static uint32_t fat32_find_free_cluster(Filesystem* filesystem) {
    if (!filesystem) {
        return 0;
    }

    FAT32Filesystem* fat32 = (FAT32Filesystem*)filesystem->filesystem_data;

    if (!fat32) {
        return 0;
    }

    uint8_t sector[512];

    for (uint32_t cluster = 2; cluster < fat32->cluster_count + 2; cluster++) {
        uint32_t fat_offset = cluster * 4;
        uint32_t fat_sector = fat32->fat_start_lba + (fat_offset / fat32->bytes_per_sector);
        uint32_t offset = fat_offset % fat32->bytes_per_sector;

        if (!block_read(filesystem->device, fat_sector, 1, sector)) {
            return 0;
        }

        uint32_t value = read_u32_le(&sector[offset]);

        if ((value & 0x0FFFFFFF) == 0) {
            return cluster;
        }
    }

    return 0;
}

static int fat32_set_cluster(Filesystem* filesystem, uint32_t cluster, uint32_t value) {
    if (!filesystem) {
        return 0;
    }

    FAT32Filesystem* fat32 = (FAT32Filesystem*)filesystem->filesystem_data;

    if (!fat32) {
        return 0;
    }

    if (cluster < 2 || cluster >= fat32->cluster_count + 2) {
        return 0;
    }

    uint32_t fat_offset = cluster * 4;
    uint32_t sector_offset = fat_offset / fat32->bytes_per_sector;
    uint32_t byte_offset = fat_offset % fat32->bytes_per_sector;
    uint8_t sector[512];

    for (uint32_t fat = 0; fat < fat32->fat_count; fat++) {
        uint32_t lba = fat32->fat_start_lba + fat * fat32->sectors_per_fat + sector_offset;

        if (!block_read(filesystem->device, lba, 1, sector)) {
            return 0;
        }

        uint32_t old_value = read_u32_le(&sector[byte_offset]);
        value = (old_value & 0xF0000000) | (value & 0x0FFFFFFF);
        write_u32_le(&sector[byte_offset], value);
        if (!block_write(filesystem->device, lba, 1, sector)) {
            return 0;
        }
    }

    return 1;
}

static int fat32_clear_cluster(Filesystem* filesystem, uint32_t cluster) {
    if (!filesystem) {
        return 0;
    }

    FAT32Filesystem* fat32 = (FAT32Filesystem*)filesystem->filesystem_data;

    if (!fat32) {
        return 0;
    }

    uint32_t lba = fat32_cluster_to_lba(fat32, cluster);

    if (lba == 0) {
        return 0;
    }

    uint8_t sector[512];

    for (uint32_t i = 0; i < 512; i++) {
        sector[i] = 0;
    }

    for (uint32_t i = 0; i < fat32->sectors_per_cluster; i++) {
        if (!block_write(filesystem->device, lba + i, 1, sector)) {
            return 0;
        }
    }

    return 1;
}

static int fat32_make_83_name(const char* name, uint8_t output[11]) {
    if (!name || !output) {
        return 0;
    }

    for (int i = 0; i < 11; i++) {
        output[i] = ' ';
    }

    int name_length = 0;

    while (name[name_length] != '\0') {
        name_length++;

        if (name_length > 12) {
            return 0;
        }
    }

    int dot = -1;

    for (int i = 0; i < name_length; i++) {
        if (name[i] == '.') {
            if (dot != -1) {
                return 0;
            }

            dot = i;
        }
    }

    int base_length = (dot == -1) ? name_length : dot;
    int extension_length = (dot == -1) ? 0 : name_length - dot - 1;

    if (base_length < 1 || base_length > 8) {
        return 0;
    }

    if (extension_length > 3) {
        return 0;
    }

    for (int i = 0; i < base_length; i++) {
        char c = name[i];

        if (c == ' ' || c == '"' || c == '*' || c == '+' || c == ',' || c == '/' || c == ':' || c == ';' || c == '<' || c == '>' || c == '=' || c == '?' || c == '\\' || c == '[' || c == ']' || c == '|') {
            return 0;
        }

        if (c >= 'a' && c <= 'z') {
            c -= 32;
        }

        output[i] = (uint8_t)c;
    }

    for (int i = 0; i < extension_length; i++) {
        char c = name[dot + 1 + i];

        if (c == ' ' || c == '"' || c == '*' || c == '+' || c == ',' || c == '/' || c == ':' || c == ';' || c == '<' || c == '>' || c == '=' || c == '?' || c == '\\' || c == '[' || c == ']' || c == '|') {
            return 0;
        }

        if (c >= 'a' && c <= 'z') {
            c -= 32;
        }

        output[8 + i] = (uint8_t)c;
    }

    return 1;
}

static int fat32_find_free_directory_entry(Filesystem* filesystem, uint32_t directory_cluster, uint32_t* output_lba, uint32_t* output_offset) {
    if (!filesystem || !output_lba || !output_offset) {
        return 0;
    }

    FAT32Filesystem* fat32 = (FAT32Filesystem*)filesystem->filesystem_data;

    if (!fat32) {
        return 0;
    }

    uint32_t cluster = directory_cluster;

    if (cluster < 2) {
        cluster = fat32->root_cluster;
    }

    uint8_t sector[512];

    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        uint32_t lba = fat32_cluster_to_lba(fat32, cluster);

        for (uint32_t s = 0; s < fat32->sectors_per_cluster; s++) {
            if (!block_read(filesystem->device, lba + s, 1, sector)) {
                return 0;
            }

            for (uint32_t offset = 0; offset < 512; offset += 32) {
                if (sector[offset] == 0x00 || sector[offset] == 0xE5) {
                    *output_lba = lba + s;
                    *output_offset = offset;
                    return 1;
                }
            }
        }

        cluster = fat32_get_next_cluster(filesystem, cluster);
    }

    return 0;
}

static int fat32_name_exists(Filesystem* filesystem, uint32_t directory_cluster, const uint8_t filename[11]) {
    if (!filesystem || !filename) {
        return 0;
    }

    FAT32Filesystem* fat32 = (FAT32Filesystem*)filesystem->filesystem_data;

    if (!fat32) {
        return 0;
    }

    uint32_t cluster = directory_cluster;

    if (cluster < 2) {
        return 0;
    }

    uint8_t sector[512];

    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        uint32_t lba = fat32_cluster_to_lba(fat32, cluster);

        if (lba == 0) {
            return 0;
        }

        for (uint32_t s = 0; s < fat32->sectors_per_cluster; s++) {
            if (!block_read(filesystem->device, lba + s, 1, sector)) {
                return 0;
            }

            for (uint32_t offset = 0; offset < 512; offset += 32) {
                uint8_t* entry = &sector[offset];

                if (entry[0] == 0x00) {
                    return 0;
                }

                if (entry[0] == 0xE5) {
                    continue;
                }

                if (entry[11] == 0x0F) {
                    continue;
                }

                int same = 1;

                for (uint32_t i = 0; i < 11; i++) {
                    if (entry[i] != filename[i]) {
                        same = 0;
                        break;
                    }
                }

                if (same) {
                    return 1;
                }
            }
        }

        cluster = fat32_get_next_cluster(filesystem, cluster);
    }

    return 0;
}

static int fat32_valid_long_name(const char* name) {
    if (!name || name[0] == '\0') {
        return 0;
    }

    uint32_t length = 0;

    while (name[length] != '\0') {
        char c = name[length];

        if (length >= FAT32_LFN_MAX_CHARS || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' || c < 32 || c > 126) {
            return 0;
        }

        length++;
    }

    if (length == 0 || fat32_name_equal(name, ".") || fat32_name_equal(name, "..")) {
        return 0;
    }

    return 1;
}

static int fat32_make_lfn_alias(Filesystem* filesystem, uint32_t directory_cluster, const char* name, uint8_t alias[11]) {
    uint32_t base_length = 0;
    uint32_t extension_start = 0;

    while (name[base_length] != '\0' && name[base_length] != '.') {
        base_length++;
    }

    if (base_length == 0) {
        return 0;
    }

    if (name[base_length] == '.') {
        extension_start = base_length + 1;
    }

    for (uint32_t attempt = 1; attempt <= 9999; attempt++) {
        for (uint32_t i = 0; i < 11; i++) {
            alias[i] = ' ';
        }

        uint32_t alias_length = base_length;

        if (alias_length > 6) {
            alias_length = 6;
        }

        for (uint32_t i = 0; i < alias_length; i++) {
            char c = name[i];

            if (c >= 'a' && c <= 'z') {
                c -= 32;
            }

            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
                alias[i] = (uint8_t)c;
            }
            else {
                alias[i] = '_';
            }
        }

        alias[alias_length++] = '~';
        uint32_t number = attempt;
        uint32_t digits = 1;

        while (number >= 10) {
            number /= 10;
            digits++;
        }

        number = attempt;

        for (uint32_t i = 0; i < digits; i++) {
            alias[alias_length + digits - i - 1] = (uint8_t)('0' + (number % 10));
            number /= 10;
        }

        if (extension_start != 0) {
            uint32_t extension_length = 0;

            while (name[extension_start + extension_length] != '\0' && extension_length < 3) {
                char c = name[extension_start + extension_length];

                if (c >= 'a' && c <= 'z') {
                    c -= 32;
                }

                alias[8 + extension_length] = (uint8_t)c;
                extension_length++;
            }
        }

        if (!fat32_name_exists(filesystem, directory_cluster, alias)) {
            return 1;
        }
    }

    return 0;
}

static int fat32_find_free_lfn_slots(Filesystem* filesystem, uint32_t directory_cluster, uint32_t required, Fat32DirectorySlot* slots) {
    FAT32Filesystem* fat32 = (FAT32Filesystem*)filesystem->filesystem_data;
    uint32_t cluster = directory_cluster;
    uint32_t run = 0;
    uint8_t sector[512];

    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        uint32_t lba = fat32_cluster_to_lba(fat32, cluster);

        for (uint32_t s =0; s < fat32->sectors_per_cluster; s++) {
            if (!block_read(filesystem->device, lba + s, 1, sector)) {
                return 0;
            }

            for (uint32_t offset = 0; offset < 512; offset += 32) {
                uint8_t first = sector[offset];

                if (first == 0x00 || first == 0xE5) {
                    if (run < required) {
                        slots[run].lba = lba + s;
                        slots[run].offset = offset;
                        run++;
                    }

                    if (run == required) {
                        return 1;
                    }
                }
                else {
                    run = 0;
                }
            }
        }

        cluster = fat32_get_next_cluster(filesystem, cluster);
    }

    return 0;
}

static void fat32_write_lfn_entry(uint8_t entry[32], const char* name, uint32_t sequence, uint32_t total, uint8_t checksum) {
    for (uint32_t i = 0; i < 32; i++) {
        entry[i] = 0xFF;
    }

    entry[0] = (uint8_t)sequence;

    if (sequence == total) {
        entry[0] |= 0x40;
    }

    entry[11] = FAT32_LFN_ATTRIBUTE;
    entry[12] = 0;
    entry[13] = checksum;
    entry[26] = 0;
    entry[27] = 0;
    const uint32_t offsets[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
    uint32_t start = (sequence - 1) * 13;

    for (uint32_t i = 0; i < 13; i++) {
        uint32_t index = start + i;
        uint16_t value = 0xFFFF;

        if (index < FAT32_LFN_MAX_CHARS && name[index] != '\0') {
            value = (uint8_t)name[index];
        }
        else if (index == 0 || name[index - 1] != '\0') {
            value = 0x0000;
        }

        write_u16_le(&entry[offsets[i]], value);
    }
}

static int fat32_write_directory_slot(Filesystem* filesystem, Fat32DirectorySlot* slot, const uint8_t entry[32]) {
    uint8_t sector[512];

    if (!block_read(filesystem->device, slot->lba, 1, sector)) {
        return 0;
    }

    for (uint32_t i = 0; i < 32; i++) {
        sector[slot->offset + i] = entry[i];
    }

    return block_write(filesystem->device, slot->lba, 1, sector);
}

static int fat32_create_name_entry(Filesystem* filesystem, uint32_t parent_cluster, const char* name, int directory) {
    if (!filesystem || !name || !fat32_valid_long_name(name)) {
        return 0;
    }

    FAT32Filesystem* fat32 = (FAT32Filesystem*)filesystem->filesystem_data;

    if (!fat32) {
        return 0;
    }

    if (parent_cluster < 2) {
        parent_cluster = fat32->root_cluster;
    }

    uint8_t alias[11];

    if (!fat32_make_lfn_alias(filesystem, parent_cluster, name, alias)) {
        return 0;
    }

    uint32_t name_length = 0;

    while(name[name_length] != '\0') {
        name_length++;
    }

    uint32_t lfn_count = (name_length + 12) / 13;
    uint32_t required_slots = lfn_count + 1;
    Fat32DirectorySlot slots[21];

    if (required_slots > 21 || !fat32_find_free_lfn_slots(filesystem, parent_cluster, required_slots, slots)) {
        return 0;
    }

    uint32_t new_cluster = 0;

    if (directory) {
        new_cluster = fat32_find_free_cluster(filesystem);

        if (new_cluster < 2 || !fat32_set_cluster(filesystem, new_cluster, 0x0FFFFFFF) || !fat32_clear_cluster(filesystem, new_cluster)) {
            return 0;
        }
    }

    uint8_t checksum = fat32_short_name_checksum(alias);

    for (uint32_t i = 0; i < lfn_count; i++) {
        uint8_t entry[32];

        fat32_write_lfn_entry(entry, name, lfn_count - i, lfn_count, checksum);

        if (!fat32_write_directory_slot(filesystem, &slots[i], entry)) {
            return 0;
        }
    }

    uint8_t short_entry[32];

    for (uint32_t i = 0; i < 32; i++) {
        short_entry[i] = 0;
    }
    
    for (uint32_t i = 0; i < 11; i++) {
        short_entry[i] = alias[i];
    }

    short_entry[11] = directory ? 0x10 : 0x20;

    if (directory) {
        write_u16_le(&short_entry[20], (uint16_t)(new_cluster >> 16));
        write_u16_le(&short_entry[26], (uint16_t)(new_cluster & 0xFFFF));
    }

    if (!fat32_write_directory_slot(filesystem, &slots[lfn_count], short_entry)) {
        return 0;
    }

    if (directory) {
        uint32_t directory_lba = fat32_cluster_to_lba(fat32, new_cluster);
        uint8_t sector[512];

        if (!block_read(filesystem->device, directory_lba, 1, sector)) {
            return 0;
        }

        for (uint32_t i = 0; i < 32; i++) {
            sector[i] = 0;
        }

        sector[0] = '.';

        for (uint32_t i = 1; i < 11; i++) {
            sector[i] = ' ';
        }

        sector[11] = 0x10;
        write_u16_le(&sector[20], (uint16_t)(new_cluster >> 16));
        write_u16_le(&sector[26], (uint16_t)(new_cluster & 0xFFFF));

        for (uint32_t i = 32; i < 64; i++) {
            sector[i] = 0;
        }

        sector[32] = '.';
        sector[33] = '.';

        for (uint32_t i = 34; i < 43; i++) {
            sector[i] = ' ';
        }

        sector[43] = 0x10;
        write_u16_le(&sector[52], (uint16_t)(parent_cluster >> 16));
        write_u16_le(&sector[58], (uint16_t)(parent_cluster & 0xFFFF));

        if (!block_write(filesystem->device, directory_lba, 1, sector)) {
            return 0;
        }
    }

    return 1;
}

int fat32_create_directory(Filesystem* filesystem, uint32_t parent_cluster, const char* name) {
    return fat32_create_name_entry(filesystem, parent_cluster, name, 1);
}

int fat32_read_directory(Filesystem* filesystem, uint32_t cluster, FilesystemEntry* entries, uint32_t max_entries, uint32_t* entry_count) {
    if (!filesystem || !entries || !entry_count) {
        return 0;
    }

    *entry_count = 0;
    FAT32Filesystem* fat32 = (FAT32Filesystem*)filesystem->filesystem_data;

    if (!fat32) {
        return 0;
    }

    if (cluster < 2) {
        cluster = fat32->root_cluster;
    }

    uint32_t current_cluster = cluster;
    char long_name[FAT32_LFN_MAX_CHARS + 1];
    uint8_t long_name_checksum = 0;
    int long_name_active = 0;
    fat32_lfn_clear(long_name);

    while (current_cluster >= 2 && current_cluster < 0x0FFFFFF8) {
        uint32_t lba = fat32_cluster_to_lba(fat32, current_cluster);
        uint32_t sectors = fat32->sectors_per_cluster;
        uint8_t sector[512];

        for (uint32_t s = 0; s < sectors; s++) {
            if (!block_read(filesystem->device, lba + s, 1, sector)) {
                return 0;
            }

            for (uint32_t offset = 0; offset < 512; offset += 32) {
                uint8_t* entry = &sector[offset];

                if (entry[0] == 0x00) {
                    return 1;
                }

                if (entry[0] == 0xE5) {
                    long_name_active = 0;
                    fat32_lfn_clear(long_name);
                    continue;
                }

                if (entry[11] == 0x0F) {
                    uint8_t sequence = entry[0] & 0x1F;

                    if (sequence == 0 || sequence > 20) {
                        long_name_active = 0;
                        fat32_lfn_clear(long_name);
                        continue;
                    }

                    if (entry[0] & 0x40) {
                        fat32_lfn_clear(long_name);
                        long_name_checksum = entry[13];
                        long_name_active = 1;
                    }

                    if (long_name_active && entry[13] == long_name_checksum) {
                        fat32_lfn_copy_entry(entry, long_name);
                    }

                    continue;
                }

                if (entry[0] == '.') {
                    continue;
                }

                if (*entry_count >= max_entries) {
                    return 1;
                }

                FilesystemEntry* output = &entries[*entry_count];
                char short_name_text[256];
                fat32_copy_name(entry, short_name_text);
                uint8_t short_name_bytes[11];

                for (uint32_t i = 0; i < 11; i++) {
                    short_name_bytes[i] = entry[i];
                }

                const char* display_name = short_name_text;

                if (long_name_active && long_name[0] != '\0' && fat32_short_name_checksum(short_name_bytes) == long_name_checksum) {
                    display_name = long_name;
                }

                uint32_t name_index = 0;

                while (display_name[name_index] != '\0' && name_index < sizeof(output->name) - 1) {
                    output->name[name_index] = display_name[name_index];
                    name_index++;
                }

                output->name[name_index] = '\0';
                output->is_directory = (entry[11] & 0x10) != 0;
                uint32_t high = (uint32_t)read_u16_le(&entry[20]);
                uint32_t low = (uint32_t)read_u16_le(&entry[26]);
                output->cluster = (high << 16) | low;
                output->size = read_u32_le(&entry[28]);
                (*entry_count)++;
                long_name_active = 0;
                fat32_lfn_clear(long_name);
            }
        }

        current_cluster = fat32_get_next_cluster(filesystem, current_cluster);
        long_name_active = 0;
        fat32_lfn_clear(long_name);
    }

    return 1;
}

int fat32_create_file(Filesystem* filesystem, uint32_t parent_cluster, const char* name) {
    return fat32_create_name_entry(filesystem, parent_cluster, name, 0);
}

int fat32_open_file(Filesystem* filesystem, const char* name, FilesystemFile* file) {
    return fat32_find_entry(filesystem, 0, name, file);
}

int fat32_read_file(FilesystemFile* file, void* buffer, uint32_t size, uint32_t* bytes_read) {
    if (!file || !file->filesystem || !buffer || !bytes_read) {
        return 0;
    }

    *bytes_read = 0;

    if (file->is_directory) {
        return 0;
    }

    if (file->position >= file->size || size == 0) {
        return 1;
    }

    uint64_t remaining = file->size - file->position;

    if ((uint64_t)size > remaining) {
        size = (uint32_t)remaining;
    }

    FAT32Filesystem* fat32 = (FAT32Filesystem*)file->filesystem->filesystem_data;

    if (!fat32 || fat32->sectors_per_cluster == 0) {
        return 0;
    }

    uint64_t cluster_size = (uint64_t)fat32->sectors_per_cluster * fat32->bytes_per_sector;
    uint64_t cluster_index = file->position / cluster_size;
    uint64_t cluster_offset = file->position % cluster_size;
    uint32_t cluster = file->first_cluster;

    if (cluster < 2) {
        return 0;
    }

    for (uint64_t i = 0; i <cluster_index; i++) {
        cluster = fat32_get_next_cluster(file->filesystem, cluster);

        if (cluster < 2 || cluster >= 0x0FFFFFF8) {
            return 0;
        }
    }

    uint8_t sector[512];

    while (*bytes_read < size) {
        uint32_t cluster_lba = fat32_cluster_to_lba(fat32, cluster);

        if (!cluster_lba) {
            return 0;
        }

        uint64_t absolute_offset = cluster_offset + *bytes_read;
        uint32_t sector_index = (uint32_t)(absolute_offset / fat32->bytes_per_sector);
        uint32_t sector_offset = (uint32_t)(absolute_offset % fat32->bytes_per_sector);

        if (sector_index >= fat32->sectors_per_cluster) {
            cluster = fat32_get_next_cluster(file->filesystem, cluster);

            if (cluster < 2 || cluster >= 0x0FFFFFF8) {
                return 0;
            }

            cluster_offset = 0;
            continue;
        }

        if (!block_read(file->filesystem->device, cluster_lba + sector_index, 1, sector)) {
            return 0;
        }

        uint32_t available = fat32->bytes_per_sector - sector_offset;
        uint32_t wanted = size - *bytes_read;

        if (available > wanted) {
            available = wanted;
        }

        for (uint32_t i = 0; i < available; i++) {
            ((uint8_t*)buffer)[*bytes_read + i] = sector[sector_offset + i];
        }

        *bytes_read += available;
    }

    file->position += *bytes_read;
    return 1;
}

int fat32_write_file(FilesystemFile* file, const void* buffer, uint32_t size, uint32_t* bytes_written) {
    if (!file || !file->filesystem || !buffer || !bytes_written) {
        return 0;
    }

    *bytes_written = 0;

    if (file->is_directory) {
        return 0;
    }

    FAT32Filesystem* fat32 = (FAT32Filesystem*)file->filesystem->filesystem_data;

    if (!fat32 || fat32->bytes_per_sector == 0 || fat32->sectors_per_cluster == 0) {
        return 0;
    }

    if (size == 0) {
        return 1;
    }

    if (file->position > UINT32_MAX || size > UINT32_MAX - file->position) {
        return 0;
    }

    uint64_t cluster_size = (uint64_t)fat32->bytes_per_sector * fat32->sectors_per_cluster;
    uint64_t end_position = file->position + size;
    uint64_t required_clusters = (end_position + cluster_size - 1) / cluster_size;
    uint32_t cluster = file->first_cluster;

    if (cluster < 2) {
        cluster = fat32_find_free_cluster(file->filesystem);

        if (cluster < 2) {
            return 0;
        }

        if (!fat32_set_cluster(file->filesystem, cluster, 0x0FFFFFFF)) {
            return 0;
        }

        if (!fat32_clear_cluster(file->filesystem, cluster)) {
            return 0;
        }

        file->first_cluster = cluster;
    }

    uint64_t existing_clusters = 1;

    while (existing_clusters < required_clusters) {
        uint32_t next = fat32_get_next_cluster(file->filesystem, cluster);

        if (next >= 0x0FFFFFF8 || next < 2) {
            next = fat32_find_free_cluster(file->filesystem);

            if (next < 2) {
                return 0;
            }

            if (!fat32_set_cluster(file->filesystem, cluster, next)) {
                return 0;
            }

            if (!fat32_set_cluster(file->filesystem, next, 0x0FFFFFFF)) {
                return 0;
            }

            if (!fat32_clear_cluster(file->filesystem, next)) {
                return 0;
            }
        }

        cluster = next;
        existing_clusters++;
    }

    cluster = file->first_cluster;
    uint64_t cluster_index = file->position / cluster_size;

    for (uint64_t i = 0; i < cluster_index; i++) {
        cluster = fat32_get_next_cluster(file->filesystem, cluster);

        if (cluster < 2 || cluster >= 0x0FFFFFF8) {
            return 0;
        }
    }

    uint64_t cluster_offset = file->position % cluster_size;
    uint8_t sector[512];

    while (*bytes_written < size) {
        uint32_t lba = fat32_cluster_to_lba(fat32, cluster);

        if (!lba) {
            return 0;
        }

        uint32_t sector_index = (uint32_t)(cluster_offset / fat32->bytes_per_sector);
        uint32_t sector_offset = (uint32_t)(cluster_offset % fat32->bytes_per_sector);

        if (sector_index >= fat32->sectors_per_cluster) {
            cluster = fat32_get_next_cluster(file->filesystem, cluster);

            if (cluster < 2 || cluster >= 0x0FFFFFF8) {
                return 0;
            }

            cluster_offset = 0;
            continue;
        }

        if (!block_read(file->filesystem->device, lba + sector_index, 1, sector)) {
            return 0;
        }

        uint32_t available = fat32->bytes_per_sector - sector_offset;
        uint32_t remaining = size - *bytes_written;

        if (available > remaining) {
            available = remaining;
        }

        for (uint32_t i = 0; i < available; i++) {
            sector[sector_offset + i] = ((const uint8_t*)buffer)[*bytes_written + i];
        }

        if (!block_write(file->filesystem->device, lba + sector_index, 1, sector)) {
            return 0;
        }

        *bytes_written += available;
        cluster_offset += available;

        if (cluster_offset >= cluster_size && *bytes_written < size) {
            cluster = fat32_get_next_cluster(file->filesystem, cluster);

            if (cluster < 2 || cluster >= 0x0FFFFFF8) {
                return 0;
            }

            cluster_offset = 0;
        }
    }

    file->position += *bytes_written;

    if (file->position > file->size) {
        file->size = file->position;
    }

    uint8_t directory_sector[512];

    if (!block_read(file->filesystem->device, file->directory_lba, 1, directory_sector)) {
        return 0;
    }

    write_u16_le(&directory_sector[file->directory_offset + 20], (uint16_t)(file->first_cluster >> 16));
    write_u16_le(&directory_sector[file->directory_offset + 26], (uint16_t)(file->first_cluster & 0xFFFF));
    write_u32_le(&directory_sector[file->directory_offset + 28], (uint32_t)file->size);

    if (!block_write(file->filesystem->device, file->directory_lba, 1, directory_sector)) {
        return 0;
    }

    return 1;
}

int fat32_open_path(Filesystem* filesystem, const char* path, FilesystemFile* file) {
    if (!filesystem || !path || !file || path[0] == '\0') {
        return 0;
    }

    FAT32Filesystem* fat32 = (FAT32Filesystem*)filesystem->filesystem_data;

    if (!fat32) {
        return 0;
    }

    uint32_t current_directory = fat32->root_cluster;
    uint32_t path_index = 0;

    while (path[path_index] == '/') {
        path_index++;
    }

    if (path[path_index] == '\0') {
        return 0;
    }

    while (1) {
        char component[FAT32_LFN_MAX_CHARS + 1];
        uint32_t component_length = 0;

        while (path[path_index] != '\0' && path[path_index] != '/') {
            if (component_length >= FAT32_LFN_MAX_CHARS) {
                return 0;
            }

            component[component_length++] = path[path_index++];
        }

        component[component_length] = '\0';

        if (component_length == 0) {
            while (path[path_index] == '/') {
                path_index++;
            }

            if (path[path_index] == '\0') {
                return 0;
            }

            continue;
        }

        FilesystemFile found;

        if (!fat32_find_entry(filesystem, current_directory, component, &found)) {
            return 0;
        }

        while (path[path_index] == '/') {
            path_index++;
        }

        if (path[path_index] == '\0') {
            *file = found;
            return 1;
        }

        if (!found.is_directory || found.first_cluster < 2) {
            return 0;
        }

        current_directory = found.first_cluster;
    }
}

static int fat32_copy_component(const char* path, uint32_t* index, char* component) {
    uint32_t length = 0;

    while (path[*index] == '/') {
        (*index)++;
    }

    while (path[*index] != '\0' && path[*index] != '/') {
        if (length >= FAT32_LFN_MAX_CHARS) {
            return 0;
        }

        component[length++] = path[*index];
        (*index)++;
    }

    component[length] = '\0';
    return length != 0;
}

static int fat32_resolve_directory(Filesystem* filesystem, const char* path, uint32_t* directory_cluster) {
    if (!filesystem || !path || !directory_cluster) {
        return 0;
    }

    FAT32Filesystem* fat32 = (FAT32Filesystem*)filesystem->filesystem_data;

    if (!fat32) {
        return 0;
    }

    uint32_t current = fat32->root_cluster;
    uint32_t index = 0;

    while (path[index] == '/') {
        index++;
    }

    if (path[index] == '\0') {
        *directory_cluster = current;
        return 1;
    }

    while (path[index] != '\0') {
        char component[FAT32_LFN_MAX_CHARS + 1];

        if (!fat32_copy_component(path, &index, component)) {
            return 0;
        }

        FilesystemFile entry;

        if (!fat32_find_entry(filesystem, current, component, &entry)) {
            return 0;
        }

        if (!entry.is_directory || entry.first_cluster < 2) {
            return 0;
        }

        current = entry.first_cluster;

        while (path[index] == '/') {
            index++;
        }
    }

    *directory_cluster = current;
    return 1;
}

static int fat32_split_parent(const char* path, char* parent, char* name) {
    if (!path || !parent || !name) {
        return 0;
    }

    uint32_t length = 0;


    while (path[length] != '\0') {
        if (length >= 255) {
            return 0;
        }

        length++;
    }

    while (length > 0 && path[length - 1] == '/') {
        length--;
    }

    if (length == 0) {
        return 0;
    }

    uint32_t slash = length;

    while (slash > 0 && path[slash - 1] != '/') {
        slash--;
    }

    uint32_t name_length = length - slash;

    if (name_length == 0 || name_length > FAT32_LFN_MAX_CHARS) {
        return 0;
    }

    for (uint32_t i = 0; i < name_length; i++) {
        name[i] = path[slash + i];
    }

    name[name_length] = '\0';

    if (slash == 0) {
        parent[0] = '/';
        parent[1] = '\0';
        return 1;
    }

    for (uint32_t i = 0; i < slash; i++) {
        parent[i] = path[i];
    }

    parent[slash] = '\0';
    return 1;
}

int fat32_create_directory_path(Filesystem* filesystem, const char* path) {
    if (!filesystem || !path) {
        return 0;
    }

    FAT32Filesystem* fat32 = (FAT32Filesystem*)filesystem->filesystem_data;

    if (!fat32) {
        return 0;
    }

    uint32_t current = fat32->root_cluster;
    uint32_t index = 0;

    while (path[index] == '/') {
        index++;
    }

    if (path[index] == '\0') {
        return 1;
    }

    while (path[index] != '\0') {
        char component[FAT32_LFN_MAX_CHARS + 1];

        if (!fat32_copy_component(path, &index, component)) {
            return 0;
        }

        FilesystemFile entry;

        if (fat32_find_entry(filesystem, current, component, &entry)) {
            if (!entry.is_directory || entry.first_cluster < 2) {
                return 0;
            }

            current = entry.first_cluster;
        }
        else {
            if (!fat32_create_directory(filesystem, current, component)) {
                return 0;
            }

            if (!fat32_find_entry(filesystem, current, component, &entry)) {
                return 0;
            }

            if (!entry.is_directory) {
                return 0;
            }

            current = entry.first_cluster;
        }

        while (path[index] == '/') {
            index++;
        }
    }

    return 1;
}

int fat32_create_file_path(Filesystem* filesystem, const char* path) {
    if (!filesystem || !path) {
        return 0;
    }

    char parent[256];
    char name[FAT32_LFN_MAX_CHARS + 1];

    if (!fat32_split_parent(path, parent, name)) {
        return 0;
    }

    uint32_t parent_cluster;

    if (!fat32_resolve_directory(filesystem, parent, &parent_cluster)) {
        return 0;
    }

    FilesystemFile existing;

    if (fat32_find_entry(filesystem, parent_cluster, name, &existing)) {
        return 0;
    }

    return fat32_create_file(filesystem, parent_cluster, name);
}

static int fat32_free_chain(Filesystem* filesystem, uint32_t first_cluster) {
    uint32_t cluster = first_cluster;

    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        uint32_t next = fat32_get_next_cluster(filesystem, cluster);

        if (!fat32_set_cluster(filesystem, cluster, 0)) {
            return 0;
        }

        if (next == cluster) {
            return 0;
        }

        cluster = next;
    }

    return 1;
}

static int fat32_mark_deleted(Filesystem* filesystem, FilesystemFile* file) {
    uint8_t sector[512];

    if (!block_read(filesystem->device, file->directory_lba, 1, sector)) {
        return 0;
    }

    sector[file->directory_offset] = 0xE5;
    return block_write(filesystem->device, file->directory_lba, 1, sector);
}


int fat32_delete_directory(Filesystem* filesystem, const char* path) {
    if (!filesystem || !path) {
        return 0;
    }

    char parent[256];
    char name[FAT32_LFN_MAX_CHARS + 1];

    if (!fat32_split_parent(path, parent, name)) {
        return 0;
    }

    uint32_t parent_cluster;

    if (!fat32_resolve_directory(filesystem, parent, &parent_cluster)) {
        return 0;
    }

    FilesystemFile directory;

    if (!fat32_find_entry(filesystem, parent_cluster, name, &directory)) {
        return 0;
    }

    if (!directory.is_directory || directory.first_cluster < 2) {
        return 0;
    }

    FilesystemEntry entries[1];
    uint32_t entry_count = 0;

    if (!fat32_read_directory(filesystem, directory.first_cluster, entries, 1, &entry_count)) {
        return 0;
    }

    if (entry_count != 0) {
        return 0;
    }

    if (!fat32_free_chain(filesystem, directory.first_cluster)) {
        return 0;
    }

    return fat32_mark_deleted(filesystem, &directory);
}

int fat32_delete_file(Filesystem* filesystem, const char* path) {
    if (!filesystem || !path) {
        return 0;
    }

    char parent[256];
    char name[FAT32_LFN_MAX_CHARS + 1];

    if (!fat32_split_parent(path, parent, name)) {
        return 0;
    }

    uint32_t parent_cluster;

    if (!fat32_resolve_directory(filesystem, parent, &parent_cluster)) {
        return 0;
    }

    FilesystemFile file;

    if (!fat32_find_entry(filesystem, parent_cluster, name, &file)) {
        return 0;
    }

    if (file.is_directory) {
        return 0;
    }

    if (file.first_cluster >= 2 && !fat32_free_chain(filesystem, file.first_cluster)) {
        return 0;
    }

    return fat32_mark_deleted(filesystem, &file);
}