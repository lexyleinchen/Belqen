#include "partition.h"
#include "../../core/log.h"

#define MBR_SECTOR_SIZE 512
#define MBR_PARTITION_TABLE_OFFSET 446
#define MBR_PARTITION_COUNT 4
#define MBR_SIGNATURE_OFFSET 510
#define MBR_SIGNATURE_LOW 0x55
#define MBR_SIGNATURE_HIGH 0xAA
#define GPT_HEADER_LBA 1
#define GPT_HEADER_SIZE 92
#define PARTITION_FIRST_LBA 2048

static BlockDevice partition_devices[4];
static PartitionDevice partition_data[4];
static int partition_registered[4] = {0, 0, 0, 0};

static uint32_t read_u32_le(const uint8_t* buffer) {
    return (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) | ((uint32_t)buffer[2] << 16) | ((uint32_t)buffer[3] << 24);
}

static void write_u32_le(uint8_t* buffer, uint32_t value) {
    buffer[0] = (uint8_t)(value & 0xFF);
    buffer[1] = (uint8_t)((value >> 8) & 0xFF);
    buffer[2] = (uint8_t)((value >> 16) & 0xFF);
    buffer[3] = (uint8_t)((value >> 24) & 0xFF);
}

static int partition_read(BlockDevice* device, uint64_t lba, uint32_t count, void* buffer) {
    if (!device || !buffer) {
        return 0;
    }

    PartitionDevice* partition = (PartitionDevice*)device->driver_data;

    if (!partition) {
        return 0;
    }

    if (lba >= partition->sector_count || count > partition->sector_count - lba) {
        return 0;
    }

    uint64_t physical_lba = partition->start_lba + lba;

    return block_read(partition->parent, physical_lba, count, buffer);
}

static int partition_write(BlockDevice* device, uint64_t lba, uint32_t count, const void* buffer) {
    if (!device || !buffer) {
        return 0;
    }

    PartitionDevice* partition = (PartitionDevice*)device->driver_data;

    if (!partition) {
        return 0;
    }
    
    if (lba >= partition->sector_count || count > partition->sector_count - lba) {
        return 0;
    }

    uint64_t physical_lba = partition->start_lba + lba;
    return block_write(partition->parent, physical_lba, count, buffer);
}

static void partition_test_read(BlockDevice* device, uint32_t partition_number) {
    uint8_t buffer[MBR_SECTOR_SIZE];
    kernel_log("partition %u testing read...", partition_number);

    if (!block_read(device, 0, 1, buffer)) {
        kernel_log("partition %u read failed.", partition_number);
        return;
    }

    kernel_log("partition %u read successful.", partition_number);

    for (int i =0; i < 16; i++) {
        kernel_log("partition%u[%u] = %u", partition_number, i, buffer[i]);
    }
}

void partition_test_create(BlockDevice* device) {
    if (!device) {
        return;
    }

    kernel_log("test creating mbr...");

    if (!partition_create_mbr(device)) {
        kernel_log("test mbr creation failed.");
        return;
    }

    uint32_t start_lba = 2048;
    uint32_t sector_count = (uint32_t)(device->sector_count - start_lba);
    if (!partition_create_mbr_partition(device, 0, start_lba, sector_count, 0x0C)) {
        kernel_log("test partition creation failed.");
        return;
    }

    kernel_log("test partition created.");
    partition_scan(device);
}

static void gpt_scan(BlockDevice* device) {
    uint8_t sector[512];
    kernel_log("trying gpt...");

    if (!block_read(device, GPT_HEADER_LBA, 1, sector)) {
        kernel_log("failed to read gpt header.");
        return;
    }

    if (sector[0] != 'E' || sector[1] != 'F' || sector[2] != 'I' || sector[3] != ' ' || sector[4] != 'P' || sector[5] != 'A' || sector[6] != 'R' || sector[7] != 'T') {
        kernel_log("no gpt header found.");
        return;
    }

    uint64_t partition_table_lba =  ((uint64_t)sector[72]) | ((uint64_t)sector[73] << 8) | ((uint64_t)sector[74] << 16) | ((uint64_t)sector[75] << 24) | ((uint64_t)sector[76] << 32) | ((uint64_t)sector[77] << 40) | ((uint64_t)sector[78] << 48) | ((uint64_t)sector[79] << 56);
    uint32_t partition_count = ((uint32_t)sector[80]) | ((uint32_t)sector[81] << 8) | ((uint32_t)sector[82] << 16) | ((uint32_t)sector[83] << 24);
    uint32_t partition_size = ((uint32_t)sector[84]) | ((uint32_t)sector[85] << 8) | ((uint32_t)sector[86] << 16) | ((uint32_t)sector[87] << 24);
    kernel_log("gpt partition table lba %u", (uint32_t)partition_table_lba);
    kernel_log("gpt partition count %u", partition_count);
    kernel_log("gpt partition size %u", partition_size);
}

static void partition_clear_registered_devices(void) {
    for (uint32_t i = 0; i < 4; i++) {
        if (!partition_registered[i]) {
            continue;
        }

        block_unregister_device(&partition_devices[i]);
        partition_registered[i] = 0;
        partition_data[i].parent = 0;
        partition_data[i].start_lba = 0;
        partition_data[i].sector_count = 0;
    }
}

void partition_scan(BlockDevice* device) {
    if (!device) {
        return;
    }

    partition_clear_registered_devices();
    kernel_log("scanning disk...");
    uint8_t sector[MBR_SECTOR_SIZE];

    if (!block_read(device, 0, 1, sector)) {
        kernel_log("faild to read mbr.");
        return;
    }

    if (sector[MBR_SIGNATURE_OFFSET] != MBR_SIGNATURE_LOW || sector[MBR_SIGNATURE_OFFSET + 1] != MBR_SIGNATURE_HIGH) {
        kernel_log("no valid mbr signature.");
        gpt_scan(device);
        return;
    }

    kernel_log("valid mbr found.");

    for (int i = 0; i < MBR_PARTITION_COUNT; i++) {
        uint32_t offset = MBR_PARTITION_TABLE_OFFSET + (i * 16);
        MBRPartition partition;
        partition.bootable = sector[offset + 0];
        partition.start_chs[0] = sector[offset + 1];
        partition.start_chs[1] = sector[offset + 2];
        partition.start_chs[2] = sector[offset + 3];
        partition.type = sector[offset + 4];
        partition.end_chs[0] = sector[offset + 5];
        partition.end_chs[1] = sector[offset + 6];
        partition.end_chs[2] = sector[offset + 7];
        partition.start_lba = read_u32_le(&sector[offset + 8]);
        partition.sector_count = read_u32_le(&sector[offset + 12]);

        if (partition.type == 0 || partition.sector_count == 0) {
            kernel_log("partition %u is empty", i);
            continue;
        }

        kernel_log("partition %u", i);
        kernel_log("type %u", partition.type);
        kernel_log("start %u", partition.start_lba);
        kernel_log("sectors %u", partition.sector_count);
        partition_data[i].parent = device;
        partition_data[i].start_lba = partition.start_lba;
        partition_data[i].sector_count = partition.sector_count;
        partition_devices[i].sector_count = partition.sector_count;
        partition_devices[i].sector_size = device->sector_size;
        partition_devices[i].type = BLOCK_DEVICE_PARTITION;
        partition_devices[i].read = partition_read;
        partition_devices[i].write = partition_write;
        partition_devices[i].driver_data = &partition_data[i];
        
        if (!partition_registered[i]) {
            block_register_device(&partition_devices[i]);
            partition_registered[i] = 1;
        }

        kernel_log("partition %u block device registered.", i);
    }
}

int partition_create_mbr(BlockDevice* device) {
    if (!device) {
        return 0;
    }

    uint8_t sector[512];

    for (int i = 0; i < 512; i++) {
        sector[i] = 0;
    }

    sector[510] = 0x55;
    sector[511] = 0xAA;

    if (!block_write(device, 0, 1, sector)) {
        kernel_log("failed to write mbr.");
        return 0;
    }

    kernel_log("empty mbr created.");
    return 1;
}

int partition_create_mbr_partition(BlockDevice* device, uint32_t partition_number, uint32_t start_lba, uint32_t sector_count, uint8_t type) {
    if (!device) {
        return 0;
    }

    if (partition_number >= 4) {
        return 0;
    }

    if (sector_count == 0) {
        return 0;
    }

    if ((uint64_t)start_lba + sector_count > device->sector_count) {
        return 0;
    }

    uint8_t sector[512];

    if (!block_read(device, 0, 1, sector)) {
        kernel_log("failed to read mbr.");
        return 0;
    }

    if (sector[510] != 0x55 || sector[511] != 0xAA) {
        kernel_log("mbr dose not exist.");
        return 0;
    }

    uint32_t offset = MBR_PARTITION_TABLE_OFFSET + partition_number * 16;
    sector[offset] = 0x00;
    sector[offset + 1] = 0;
    sector[offset + 2] = 0;
    sector[offset + 3] = 0;
    sector[offset + 4] = type;
    sector[offset + 5] = 0;
    sector[offset + 6] = 0;
    sector[offset + 7] = 0;
    write_u32_le(&sector[offset + 8], start_lba);
    write_u32_le(&sector[offset + 12], sector_count);
    
    if (!block_write(device, 0, 1, sector)) {
        kernel_log("failed to write partition.");
        return 0;
    }

    kernel_log("mbr partition created.");
    return 1;
}

int partition_create_mbr_partition_size(BlockDevice* device, uint32_t size_mib, uint8_t type, uint32_t* created_partition) {
    if (!device || size_mib == 0 || !created_partition) {
        return 0;
    }

    uint64_t bytes = (uint64_t)size_mib * 1024 * 1024;
    uint32_t sectors = (uint32_t)(bytes / device->sector_size);

    if (sectors == 0) {
        return 0;
    }

    uint32_t start_lba;

    if (!partition_find_free_space(device, sectors, &start_lba)) {
        kernel_log("not enough free space.");
        return 0;
    }

    for (uint32_t i = 0; i < 4; i++) {
        MBRPartitionInfo partition;

        if (!partition_get_mbr_partition(device, i, &partition)) {
            return 0;
        }

        if (partition.type == 0 || partition.sector_count == 0) {
            if (partition_create_mbr_partition(device, i, start_lba, sectors, type)) {
                *created_partition = i;
                return 1;
            }
        }
    }

    kernel_log("no free mbr entries.");
    return 0;
}

int partition_delete(BlockDevice* device, uint32_t partition_number) {
    if (!device) {
        return 0;
    }

    if (partition_number >= MBR_PARTITION_COUNT) {
        return 0;
    }

    uint8_t sector[MBR_SECTOR_SIZE];

    if (!block_read(device, 0, 1, sector)) {
        kernel_log("failed to read mbr.");
        return 0;
    }

    if (sector[MBR_SIGNATURE_OFFSET] != MBR_SIGNATURE_LOW || sector[MBR_SIGNATURE_OFFSET + 1] != MBR_SIGNATURE_HIGH) {
        kernel_log("no valid mbr.");
        return 0;
    }

    uint32_t offset = MBR_PARTITION_TABLE_OFFSET + partition_number * 16;

    for (uint32_t i = 0; i < 16; i++) {
        sector[offset + i] = 0;
    }

    if (!block_write(device, 0, 1, sector)) {
        kernel_log("failed to delete partition.");
        return 0;
    }

    kernel_log("partition %u deleted.", partition_number);
    return 1;
}

int partition_get_mbr_partition(BlockDevice* device, uint32_t partition_number, MBRPartitionInfo* partition) {
    if (!device || !partition) {
        return 0;
    }

    if (partition_number >= MBR_PARTITION_COUNT) {
        return 0;
    }

    uint8_t sector[MBR_SECTOR_SIZE];

    if (!block_read(device, 0, 1, sector)) {
        return 0;
    }

    if (sector[510] != 0x55 || sector[511] != 0xAA) {
        return 0;
    }

    uint32_t offset = MBR_PARTITION_TABLE_OFFSET + partition_number * 16;
    partition->bootable = sector[offset];
    partition->type = sector[offset + 4];
    partition->start_lba = read_u32_le(&sector[offset + 8]);
    partition->sector_count = read_u32_le(&sector[offset + 12]);
    return 1;
}

int partition_find_free_space(BlockDevice* device, uint32_t requested_sectors, uint32_t* start_lba) {
    if (!device || !start_lba) {
        return 0;
    }

    if (requested_sectors == 0) {
        return 0;
    }

    uint32_t current_lba = PARTITION_FIRST_LBA;

    for (uint32_t i = 0; i < MBR_PARTITION_COUNT; i++) {
        MBRPartitionInfo partition;

        if (!partition_get_mbr_partition(device, i, &partition)) {
            return 0;
        }

        if (partition.type == 0 || partition.sector_count == 0) {
            continue;
        }

        if (current_lba + requested_sectors <= partition.start_lba) {
            *start_lba = current_lba;
            return 1;
        }

        uint32_t partition_end = partition.start_lba + partition.sector_count;

        if (partition_end > current_lba) {
            current_lba = partition_end;
        }

        current_lba = (current_lba + 2047) & ~2047;
    }

    if ((uint64_t)current_lba + requested_sectors <= device->sector_count) {
        *start_lba = current_lba;
        return 1;
    }

    return 0;
}

int partition_has_mbr(BlockDevice* device) {
    if (!device) {
        return 0;
    }

    uint8_t sector[MBR_SECTOR_SIZE];

    if (!block_read(device, 0, 1, sector)) {
        return 0;
    }

    if (sector[MBR_SIGNATURE_OFFSET] != MBR_SIGNATURE_LOW || sector[MBR_SIGNATURE_OFFSET + 1] != MBR_SIGNATURE_HIGH) {
        return 0;
    }

    return 1;
}

BlockDevice* partition_get_device(BlockDevice* device, uint32_t partition_number) {
    if (!device) {
        return 0;
    }

    if (partition_number >= MBR_PARTITION_COUNT) {
        return 0;
    }

    MBRPartitionInfo partition;

    if (!partition_get_mbr_partition(device, partition_number, &partition)) {
        return 0;
    }

    if (partition.type == 0 || partition.sector_count == 0) {
        return 0;
    }

    partition_data[partition_number].parent = device;
    partition_data[partition_number].start_lba = partition.start_lba;
    partition_data[partition_number].sector_count = partition.sector_count;
    partition_devices[partition_number].sector_count = partition.sector_count;
    partition_devices[partition_number].sector_size = device->sector_size;
    partition_devices[partition_number].type = BLOCK_DEVICE_PARTITION;
    partition_devices[partition_number].read = partition_read;
    partition_devices[partition_number].write = partition_write;
    partition_devices[partition_number].driver_data = &partition_data[partition_number];

    if (!partition_registered[partition_number]) {
        block_register_device(&partition_devices[partition_number]);
        partition_registered[partition_number] = 1;
    }

    return &partition_devices[partition_number];
}