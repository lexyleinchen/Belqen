#include "fat32.h"

#include "../../../../core/log.h"
#include "../../../../core/task.h"

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
                    continue;
                }

                if (entry[11] == 0x0F) {
                    continue;
                }

                if (entry[0] == '.') {
                    continue;
                }

                if (*entry_count >= max_entries) {
                    return 1;
                }

                FilesystemEntry* output = &entries[*entry_count];
                fat32_copy_name(entry, output->name);
                output->is_directory = (entry[11] & 0x10) != 0;
                uint32_t high = (uint32_t)read_u16_le(&entry[20]);
                uint32_t low = (uint32_t)read_u16_le(&entry[26]);
                output->cluster = (high << 16) | low;
                output->size = read_u32_le(&entry[28]);
                (*entry_count)++;
            }
        }

        current_cluster = fat32_get_next_cluster(filesystem, current_cluster);
    }

    return 1;
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

int fat32_create_directory(Filesystem* filesystem, uint32_t parent_cluster, const char* name) {
    if (!filesystem || !name) {
        return 0;
    }


    FAT32Filesystem* fat32 = (FAT32Filesystem*)filesystem->filesystem_data;

    if (!fat32) {
        return 0;
    }

    uint8_t filename[11];

    if (!fat32_make_83_name(name, filename)) {
        kernel_log("fat32 invalid directory name.");
        return 0;
    }

    uint32_t new_cluster = fat32_find_free_cluster(filesystem);

    if (new_cluster == 0) {
        kernel_log("fat32 no free clusters.");
        return 0;
    }

    if (!fat32_set_cluster(filesystem, new_cluster, 0x0FFFFFFF)) {
        return 0;
    }

    if (!fat32_clear_cluster(filesystem, new_cluster)) {
        fat32_set_cluster(filesystem, new_cluster, 0);
        return 0;
    }

    uint32_t lba;
    uint32_t offset;

    if (!fat32_find_free_directory_entry(filesystem, parent_cluster, &lba, &offset)) {
        fat32_set_cluster(filesystem, new_cluster, 0);
        kernel_log("fat32 parent directory is full.");
        return 0;
    }

    uint32_t new_lba = fat32_cluster_to_lba(fat32, new_cluster);
    uint8_t sector[512];

    if (!block_read(filesystem->device, new_lba, 1, sector)) {
        fat32_set_cluster(filesystem, new_cluster, 0);
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
    uint32_t parent = parent_cluster;

    if (parent < 2) {
        parent = fat32->root_cluster;
    }

    write_u16_le(&sector[52], (uint16_t)(parent >> 16));
    write_u16_le(&sector[58], (uint16_t)(parent & 0xFFFF));

    if (!block_write(filesystem->device, new_lba, 1, sector)) {
        return 0;
    }

    if (!block_read(filesystem->device, lba, 1, sector)) {
        fat32_set_cluster(filesystem, new_cluster, 0);
        return 0;
    }

    for (uint32_t i = 0; i < 32; i++) {
        sector[offset + i] = 0;
    }

    for (uint32_t i = 0; i < 11; i++) {
        sector[offset + i] = filename[i];
    }

    sector[offset + 11] = 0x10;
    write_u16_le(&sector[offset + 20], (uint16_t)(new_cluster >> 16));
    write_u16_le(&sector[offset + 26], (uint16_t)(new_cluster & 0xFFFF));
    write_u32_le(&sector[offset + 28], 0);

    if (!block_write(filesystem->device, lba, 1, sector)) {
        fat32_set_cluster(filesystem, new_cluster, 0);
        kernel_log("fat32 failed to create directory.");
        return 0;
    }

    kernel_log("fat32 directory created %s cluster %u", name, parent_cluster);
    return 1;
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

int fat32_create_file(Filesystem* filesystem, uint32_t parent_cluster, const char* name) {
    if (!filesystem || !name) {
        return 0;
    }

    FAT32Filesystem* fat32 = (FAT32Filesystem*)filesystem->filesystem_data;

    if (!fat32) {
        return 0;
    }

    uint8_t filename[11];

    if (!fat32_make_83_name(name, filename)) {
        kernel_log("fat32 invalid 8.3 filename.");
        return 0;
    }

    uint32_t directory = parent_cluster;

    if (directory < 2) {
        directory = fat32->root_cluster;
    }

    if (fat32_name_exists(filesystem, directory, filename)) {
        kernel_log("fat32 file already exists.");
        return 0;
    }

    uint32_t lba;
    uint32_t offset;

    if (!fat32_find_free_directory_entry(filesystem, directory, &lba, &offset)) {
        kernel_log("fat32 directory is full.");
        return 0;
    }

    uint8_t sector[512];

    if (!block_read(filesystem->device, lba, 1, sector)) {
        return 0;
    }

    for (uint32_t i = 0; i < 32; i++) {
        sector[offset + i] = 0;
    }

    for (uint32_t i = 0; i < 11; i++) {
        sector[offset + i] = filename[i];
    }

    sector[offset + 11] = 0x20;
    write_u16_le(&sector[offset + 20], 0);
    write_u16_le(&sector[offset + 26], 0);
    write_u32_le(&sector[offset + 28], 0);

    if (!block_write(filesystem->device, lba, 1, sector)) {
        kernel_log("fat32 failed to create file.");
        return 0;
    }

    kernel_log("fat32 file created %s", name);
    return 1;
}