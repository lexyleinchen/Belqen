#include "ide.h"
#include "../../storage/block.h"
#include "../../interrupts/interrupts.h"
#include "../../interrupts/apic.h"
#include "../../core/log.h"

#define IDE_PRIMARY_IO 0x1F0
#define IDE_PRIMARY_CONTROL 0x3F6
#define ATA_REG_DATA 0
#define ATA_REG_ERROR 1
#define ATA_REG_FEATURES 1
#define ATA_REG_SECCOUNT0 2
#define ATA_REG_LBA0 3
#define ATA_REG_LBA1 4
#define ATA_REG_LBA2 5
#define ATA_REG_HDDEVSEL 6
#define ATA_REG_COMMAND 7
#define ATA_REG_STATUS 7
#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_READ_PIO 0x20
#define ATA_CMD_WRITE_PIO 0x30
#define ATA_STATUS_ERR 0x01
#define ATA_STATUS_DRQ 0x08
#define ATA_STATUS_SRV 0x10
#define ATA_STATUS_DF 0x20
#define ATA_STATUS_RDY 0x40
#define ATA_STATUS_BSY 0x80
#define IDE_SECTOR_SIZE 512

static int ide_disk_present = 0;
static uint32_t ide_sector_count = 0;
static uint16_t ide_io_base = IDE_PRIMARY_IO;
static uint16_t ide_control_base = IDE_PRIMARY_CONTROL;
static BlockDevice ide_block_device;
static volatile uint32_t ide_operation_complete = 0;
static volatile uint32_t ide_operation_error = 0;

static void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static uint16_t inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void ide_400ns_delay(void) {
    inb(ide_control_base);
    inb(ide_control_base);
    inb(ide_control_base);
    inb(ide_control_base);
}

static int ide_identify(void) {
    uint8_t status;
    uint16_t identify_word[256];
    kernel_log("identifying primary master...");
    outb(ide_io_base + ATA_REG_HDDEVSEL, 0xA0);
    ide_400ns_delay();
    outb(ide_io_base + ATA_REG_SECCOUNT0, 0);
    outb(ide_io_base + ATA_REG_LBA0, 0);
    outb(ide_io_base + ATA_REG_LBA1, 0);
    outb(ide_io_base + ATA_REG_LBA2, 0);
    outb(ide_io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    status = inb(ide_io_base + ATA_REG_STATUS);

    if (status == 0) {
        kernel_log("no ide device found.");
        return 0;
    }

    while (status & ATA_STATUS_BSY) {
        status = inb(ide_io_base + ATA_REG_STATUS);
    }

    if (status & ATA_STATUS_ERR) {
        kernel_log("ide device error.");
        return 0;
    }

    while (!(status & ATA_STATUS_DRQ)) {
        status = inb(ide_io_base + ATA_REG_STATUS);

        if (status & ATA_STATUS_ERR) {
            kernel_log("ide device error.");
            return 0;
        }
    }

    for (int i = 0; i < 256; i++) {
        identify_word[i] = inw(ide_io_base + ATA_REG_DATA);
    }

    ide_sector_count = ((uint32_t)identify_word[61] << 16) | identify_word[60];

    if (ide_sector_count == 0) {
        kernel_log("ide device has zero sectors.");
        return 0;
    }

    kernel_log("ide device identified. sector count: %u", ide_sector_count);
    ide_disk_present = 1;
    return 1;
}

static int ide_wait_for_interrupt(void) {
    uint32_t timeout = 1000000;

    while (!ide_operation_complete && timeout > 0) {
        __asm__ volatile ("hlt");
        uint32_t irr = apic_read_register(0x200 + ((46 / 32) * 0x10));
        timeout--;
    }

    if (!ide_operation_complete) {
        kernel_log("Ide interrupt timeout.");
        return 0;
    }

    if (ide_operation_error) {
        kernel_log("Ide operation failed.");
        return 0;
    }

    return 1;
}

static int ide_read(BlockDevice* device, uint64_t lba, uint32_t count, void* buffer) {
    if (!ide_disk_present) {
        return 0;
    }

    if (!device || !buffer) {
        return 0;
    }

    if (count == 0) {
        return 1;
    }

    if (lba + count > ide_sector_count) {
        return 0;
    }

    uint8_t* destination = (uint8_t*)buffer;

    for (uint32_t sector = 0; sector < count; sector++) {
        uint32_t current_lba = (uint32_t)(lba + sector);
        uint8_t status = inb(ide_io_base + ATA_REG_STATUS);
        
        while (status & ATA_STATUS_BSY) {
            status = inb(ide_io_base + ATA_REG_STATUS);
        }

        outb(ide_io_base + ATA_REG_HDDEVSEL, 0xE0 | ((current_lba >> 24) & 0x0F));
        ide_400ns_delay();
        outb(ide_io_base + ATA_REG_SECCOUNT0, 1);
        outb(ide_io_base + ATA_REG_LBA0, current_lba & 0xFF);
        outb(ide_io_base + ATA_REG_LBA1, (current_lba >> 8) & 0xFF);
        outb(ide_io_base + ATA_REG_LBA2, (current_lba >> 16) & 0xFF);
        ide_operation_complete = 0;
        ide_operation_error = 0;
        apic_dump_redirection(14);
        outb(ide_io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

        if (!ide_wait_for_interrupt()) {
            return 0;
        }

        status = inb(ide_io_base + ATA_REG_STATUS);

        if (!(status & ATA_STATUS_DRQ)) {
            kernel_log("Ide read data no ready.");
            return 0;
        }

        for (uint32_t i = 0; i < 256; i++) {
            uint16_t data = inw(ide_io_base + ATA_REG_DATA);
            destination[sector * IDE_SECTOR_SIZE + i * 2] = (uint8_t)(data & 0xFF);
            destination[sector * IDE_SECTOR_SIZE + i * 2 + 1] = (uint8_t)(data >> 8);
        }
    }

    return 1;
}

static int ide_write(BlockDevice* device, uint64_t lba, uint32_t count, const void* buffer) {
    if (!ide_disk_present) {
        return 0;
    }

    if (!device || !buffer) {
        return 0;
    }

    if (count == 0) {
        return 1;
    }

    if (lba + count > ide_sector_count) {
        return 0;
    }

    const uint8_t* source = (const uint8_t*)buffer;

    for (uint32_t sector = 0; sector < count; sector++) {
        uint32_t current_lba = (uint32_t)(lba + sector);
        uint8_t status;
        int timeout = 1000000;

        while (timeout-- > 0) {
            status = inb(ide_io_base + ATA_REG_STATUS);

            if (!(status & ATA_STATUS_BSY)) {
                break;
            }
        }

        if (timeout <= 0) {
            kernel_log("ide write timeout waiting for drive.");
            return 0;
        }

        outb(ide_io_base + ATA_REG_HDDEVSEL, 0xE0 | ((current_lba >> 24) & 0x0F));
        ide_400ns_delay();
        outb(ide_io_base + ATA_REG_SECCOUNT0, 1);
        outb(ide_io_base + ATA_REG_LBA0, current_lba & 0xFF);
        outb(ide_io_base + ATA_REG_LBA1, (current_lba >> 8) & 0xFF);
        outb(ide_io_base + ATA_REG_LBA2, (current_lba >> 16) & 0xFF);
        ide_operation_complete = 0;
        ide_operation_error = 0;
        outb(ide_io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

        for (uint32_t i = 0; i < 256; i++) {
            uint32_t offset = sector * IDE_SECTOR_SIZE + i * 2;
            uint16_t word = (uint16_t)source[offset] | ((uint16_t)source[offset + 1] << 8);
            outw(ide_io_base +ATA_REG_DATA, word);
        } 
        
        if (!ide_wait_for_interrupt()) {
            return 0;
        }
    }
    
    return 1;
}

static void ide_test_read(void) {
    uint8_t buffer[IDE_SECTOR_SIZE];
    kernel_log("testing ide read...");
    int result = block_read(&ide_block_device, 0, 1, buffer);

    if (!result) {
        kernel_log("ide read test failed.");
        return;
    }

    kernel_log("ide read test successful. first sector data:");
    for (int i = 0; i < 16; i++) {
        kernel_log("sector0[%d] = %u", i, buffer[i]);
    }
}

static void ide_test_write(void) {
    uint8_t write_buffer[IDE_SECTOR_SIZE];
    uint8_t read_buffer[IDE_SECTOR_SIZE];

    for (int i = 0; i < IDE_SECTOR_SIZE; i++) {
        write_buffer[i] = 0;
        read_buffer[i] = 0;
    }

    uint64_t test_lba = 100;
    write_buffer[0] = 'P';
    write_buffer[1] = 'r';
    write_buffer[2] = 'i';
    write_buffer[3] = 'n';
    write_buffer[4] = 't';
    write_buffer[5] = 'O';
    write_buffer[6] = 'S';
    kernel_log("ide testing sector write...");

    if (!block_write(&ide_block_device, test_lba, 1, write_buffer)) {
        kernel_log("ide sector write failed.");
        return;
    }

    kernel_log("ide sector write successful.");

    if (!block_read(&ide_block_device, test_lba, 1, read_buffer)) {
        kernel_log("ide sector read failed.");
        return;
    }

    if (read_buffer[0] == 'P' && read_buffer[1] == 'r' && read_buffer[2] == 'i' && read_buffer[3] == 'n' && read_buffer[4] == 't' && read_buffer[5] == 'O' && read_buffer[6] == 'S') {
        kernel_log("ide write verification successfull.");
    }
    else {
        kernel_log("ide write verification failed.");
    }
}

void ide_controller_found(uint16_t bus, uint8_t slot, uint8_t function, uint8_t prog_if) {
    kernel_log("found ide controller = bus: %u | slot: %u | function: %u | prog_if %u", bus, slot, function, prog_if);
    ide_io_base = IDE_PRIMARY_IO;
    ide_control_base = IDE_PRIMARY_CONTROL;
    kernel_log("ide io base = %u", ide_io_base);
    kernel_log("ide control base = %u", ide_control_base);
}

void ide_init(void) {
    kernel_log("initializing ide...");
    if (!ide_identify()) {
        kernel_log("no usable disk found.");
        return;
    }

    ide_block_device.sector_count = ide_sector_count;
    ide_block_device.sector_size = IDE_SECTOR_SIZE;
    ide_block_device.type = BLOCK_DEVICE_PHYSICAL;
    ide_block_device.read = ide_read;
    ide_block_device.write = ide_write;
    ide_block_device.driver_data = 0;
    block_register_device(&ide_block_device);
    kernel_log("ide initialized.");
}

void ide_enable_interrupts(void) {
    outb(ide_control_base, 0x00);
    interrupt_register_irq(14, ide_interrupt);
    interrupt_unmask_irq(14);
    kernel_log("ide interrupts enabled.");
}

void ide_interrupt(void) {
    uint8_t status = inb(ide_io_base + ATA_REG_STATUS);

    if (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) {
        ide_operation_error = 1;
    }

    ide_operation_complete = 1;
}